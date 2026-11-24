#include "lumenanimations.h"
#include "lumenstyle.h"
#include "lumenconfig.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSlider>
#include <QStyleOption>
#include <QTabBar>
#include <QTabWidget>
#include <QWindow>

#include <cmath>
#include <tuple>

/// Translucent only with a window that has alpha: on an opaque buffer, see-through fills turn black and pile up.
static bool translucentSurface(const QWidget *w)
{
    if (!Lumen::Config::get().materials || !Lumen::Config::get().translucentMenus || !w || !w->testAttribute(Qt::WA_TranslucentBackground)) {
        return false;
    }
    const QWindow *win = w->window()->windowHandle();
    return !win || win->format().hasAlpha();
}

namespace Lumen
{
namespace
{
inline qreal controlRadius()
{
    return Config::get().controlRadius;
}

enum AnimKey : quintptr { KeyHover = 1, KeyCheck, KeyRadioDot, KeySlider, KeyScroll, KeyTab, KeyInput, KeyItem };

struct Pal {
    bool dark;
    const T::Fluent &f;
    QColor accent;
    QColor fill;
    QColor onAccent;
};

Pal pal(const QStyleOption *o)
{
    const bool d = isDark(o->palette);
    const QColor accent = o->palette.color(QPalette::Highlight);
    return {d, fluent(d), accent, accentFill(accent, d), argb(fluent(d).textOnAccent)};
}

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    if (t <= 0) {
        return a;
    }
    if (t >= 1) {
        return b;
    }
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QRectF strokeRect(const QRect &r, qreal w = 1.0)
{
    return QRectF(r).adjusted(w / 2, w / 2, -w / 2, -w / 2);
}

void roundRect(QPainter *p, const QRectF &r, qreal radius, const QBrush &fill, const QPen &pen = Qt::NoPen)
{
    p->setPen(pen);
    p->setBrush(fill);
    p->drawRoundedRect(r, radius, radius);
}

/// 1px border; `edge` (if valid) shades the top (edgeTop) or bottom 3px like Fluent's elevation stroke.
struct Border {
    QColor color;
    QColor edge = {};
    bool edgeTop = false;
};

Border elevation(const Pal &c, bool onAccent)
{
    return {argb(onAccent ? c.f.controlStrokeOnAccent : c.f.controlStroke),
            argb(onAccent ? c.f.controlStrokeOnAccentSecondary : c.f.controlStrokeSecondary),
            c.dark && !onAccent};
}

QPen borderPen(const Border &b, const QRectF &r)
{
    if (!b.color.isValid() || b.color.alpha() == 0) {
        return Qt::NoPen;
    }
    if (!b.edge.isValid()) {
        return QPen(b.color, 1.0);
    }
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    const qreal e = std::min(1.0, 3.0 / std::max(1.0, r.height()));
    g.setColorAt(0, b.edgeTop ? b.edge : b.color);
    g.setColorAt(b.edgeTop ? e : 1 - e, b.color);
    g.setColorAt(1, b.edgeTop ? b.color : b.edge);
    return QPen(QBrush(g), 1.0);
}

/// Rounded panel filling the integer rect `outer` (fill + 1px border). Corners are antialiased once into a
/// cached kit; edges stretch 1px slices and the interior is a plain fill, so big frames cost a few blits.
void panel(QPainter *p, const QRect &outer, qreal radius, const QColor &fill, const Border &b = {})
{
    const int c = int(std::ceil(radius)) + 1;
    const int k = 2 * c + 1;
    if (radius <= 0 || outer.width() < k || outer.height() < k || p->worldTransform().type() > QTransform::TxTranslate) {
        const QRectF r = strokeRect(outer);
        roundRect(p, r, radius, fill.alpha() ? QBrush(fill) : QBrush(Qt::NoBrush), borderPen(b, r));
        return;
    }
    const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
    const size_t hash = qHashMulti(0, radius, fill.rgba(), b.color.isValid() ? b.color.rgba() : 0u, b.edge.isValid() ? b.edge.rgba() : 0u, b.edgeTop, dpr);
    const QString key = QStringLiteral("lumen-panel-") + QString::number(quint64(hash), 36);
    QPixmap kit;
    if (!QPixmapCache::find(key, &kit)) {
        QImage img(QSize(k, k) * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter kp(&img);
        kp.setRenderHint(QPainter::Antialiasing);
        const QRectF r = strokeRect(QRect(0, 0, k, k));
        roundRect(&kp, r, radius, fill.alpha() ? QBrush(fill) : QBrush(Qt::NoBrush), borderPen(b, r));
        kp.end();
        kit = QPixmap::fromImage(std::move(img));
        QPixmapCache::insert(key, kit);
    }
    const auto src = [dpr](int x, int y, int w, int h) {
        return QRectF(x * dpr, y * dpr, w * dpr, h * dpr);
    };
    const bool aa = p->testRenderHint(QPainter::Antialiasing);
    const bool smooth = p->testRenderHint(QPainter::SmoothPixmapTransform);
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    const int x0 = outer.left(), y0 = outer.top(), w = outer.width(), h = outer.height();
    const int x1 = x0 + w - c, y1 = y0 + h - c, iw = w - 2 * c, ih = h - 2 * c;
    p->drawPixmap(QRectF(x0, y0, c, c), kit, src(0, 0, c, c));
    p->drawPixmap(QRectF(x1, y0, c, c), kit, src(k - c, 0, c, c));
    p->drawPixmap(QRectF(x0, y1, c, c), kit, src(0, k - c, c, c));
    p->drawPixmap(QRectF(x1, y1, c, c), kit, src(k - c, k - c, c, c));
    if (iw > 0) {
        p->drawPixmap(QRectF(x0 + c, y0, iw, c), kit, src(c, 0, 1, c));
        p->drawPixmap(QRectF(x0 + c, y1, iw, c), kit, src(c, k - c, 1, c));
    }
    if (ih > 0) {
        p->drawPixmap(QRectF(x0, y0 + c, c, ih), kit, src(0, c, c, 1));
        p->drawPixmap(QRectF(x1, y0 + c, c, ih), kit, src(k - c, c, c, 1));
    }
    if (iw > 0 && ih > 0 && fill.alpha() > 0) {
        p->fillRect(QRect(x0 + c, y0 + c, iw, ih), fill);
    }
    p->setRenderHint(QPainter::Antialiasing, aa);
    p->setRenderHint(QPainter::SmoothPixmapTransform, smooth);
}

/// Fills the stroke outline instead of using a pen: Qt draws pens of width <= 1 with a cosmetic
/// line rasterizer that turns diagonals into jagged pixel pairs and double-blends crossings.
void strokeFill(QPainter *p, const QPainterPath &path, const QColor &color, qreal width, Qt::PenCapStyle cap = Qt::RoundCap)
{
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    stroker.setCapStyle(cap);
    stroker.setJoinStyle(Qt::RoundJoin);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->fillPath(stroker.createStroke(path).simplified(), color);
    p->restore();
}

void chevron(QPainter *p, const QRectF &box, Qt::ArrowType dir, const QColor &color, qreal w = 1.0)
{
    const QPointF c = box.center();
    const qreal s = std::min(box.width(), box.height()) / 2;
    const qreal h = s / 2;
    QPainterPath path;
    switch (dir) {
    case Qt::UpArrow:
        path.moveTo(c.x() - s, c.y() + h);
        path.lineTo(c.x(), c.y() - h);
        path.lineTo(c.x() + s, c.y() + h);
        break;
    case Qt::DownArrow:
        path.moveTo(c.x() - s, c.y() - h);
        path.lineTo(c.x(), c.y() + h);
        path.lineTo(c.x() + s, c.y() - h);
        break;
    case Qt::LeftArrow:
        path.moveTo(c.x() + h, c.y() - s);
        path.lineTo(c.x() - h, c.y());
        path.lineTo(c.x() + h, c.y() + s);
        break;
    case Qt::RightArrow:
        path.moveTo(c.x() - h, c.y() - s);
        path.lineTo(c.x() + h, c.y());
        path.lineTo(c.x() - h, c.y() + s);
        break;
    default:
        return;
    }
    strokeFill(p, path, color, w);
}

void checkmark(QPainter *p, const QRectF &box, const QColor &color)
{
    QPainterPath path;
    path.moveTo(box.left() + box.width() * 0.18, box.top() + box.height() * 0.52);
    path.lineTo(box.left() + box.width() * 0.40, box.top() + box.height() * 0.74);
    path.lineTo(box.left() + box.width() * 0.82, box.top() + box.height() * 0.30);
    QPen pen(color, 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
}

void focusRing(QPainter *p, const QRect &r, qreal radius, const Pal &c)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    roundRect(p, strokeRect(r, 2), radius, Qt::NoBrush, QPen(argb(c.f.focusOuter), 2));
    roundRect(p, strokeRect(r.adjusted(2, 2, -2, -2), 1), std::max(0.0, radius - 2), Qt::NoBrush, QPen(argb(c.f.focusInner), 1));
    p->restore();
}

bool keyboardFocus(const QStyleOption *o)
{
    return (o->state & QStyle::State_HasFocus) && (o->state & QStyle::State_KeyboardFocusChange);
}

QPainterPath roundedPath(const QRectF &r, qreal tl, qreal tr, qreal br, qreal bl)
{
    QPainterPath path;
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    path.arcTo(QRectF(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr), 90, -90);
    path.lineTo(r.right(), r.bottom() - br);
    path.arcTo(QRectF(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
    path.lineTo(r.left() + bl, r.bottom());
    path.arcTo(QRectF(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
    path.lineTo(r.left(), r.top() + tl);
    path.arcTo(QRectF(r.left(), r.top(), 2 * tl, 2 * tl), 180, -90);
    path.closeSubpath();
    return path;
}
}

void Style::drawButtonPanel(const QStyleOption *opt, QPainter *p, const QWidget *w, bool accent, bool flat) const
{
    const Pal c = pal(opt);
    const bool enabled = opt->state & State_Enabled;
    const bool pressed = opt->state & (State_Sunken | State_On);
    const qreal hv = anim(opt, w, KeyHover, enabled && (opt->state & State_MouseOver));
    const qreal radius = controlRadius();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (flat) {
        const QColor fill = pressed ? argb(c.f.subtleFillPressed) : withAlpha(argb(c.f.subtleFillHover), hv);
        if (fill.alpha() > 0) {
            panel(p, opt->rect, radius, fill);
        }
    } else if (accent && enabled) {
        const QColor fill = withAlpha(c.fill, pressed ? 0.8 : 1.0 - 0.1 * hv);
        panel(p, opt->rect, radius, fill, pressed ? Border{argb(c.f.controlStrokeOnAccent)} : elevation(c, true));
    } else if (!enabled) {
        panel(p, opt->rect, radius, argb(accent ? c.f.accentDisabled : c.f.controlFillDisabled), accent ? Border{} : Border{argb(c.f.controlStroke)});
    } else {
        const QColor fill = pressed ? argb(c.f.controlFillPressed) : mix(argb(c.f.controlFill), argb(c.f.controlFillHover), hv);
        panel(p, opt->rect, radius, fill, pressed ? Border{argb(c.f.controlStroke)} : elevation(c, false));
    }
    p->restore();
    if (keyboardFocus(opt)) {
        focusRing(p, opt->rect, radius + 1, c);
    }
}

void Style::drawInputPanel(const QStyleOption *opt, QPainter *p, const QWidget *w, bool focus) const
{
    const Pal c = pal(opt);
    const bool enabled = opt->state & State_Enabled;
    const qreal hv = anim(opt, w, KeyInput, enabled && (opt->state & State_MouseOver) && !focus);
    const qreal radius = controlRadius();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QColor fill = !enabled ? argb(c.f.controlFillDisabled) : focus ? argb(c.f.controlFillInputActive) : mix(argb(c.f.controlFill), argb(c.f.controlFillHover), hv);
    panel(p, opt->rect, radius, fill, {argb(c.f.controlStroke)});
    QPainterPath clip;
    clip.addRoundedRect(QRectF(opt->rect), radius, radius);
    p->setClipPath(clip);
    const qreal line = focus ? T::Size::InputUnderline : T::Size::Border;
    const QColor lineColor = !enabled ? argb(c.f.controlStroke) : focus ? c.fill : argb(c.f.controlStrongStroke);
    p->fillRect(QRectF(opt->rect.left(), opt->rect.bottom() + 1 - line, opt->rect.width(), line), lineColor);
    p->restore();
}

void Style::drawCheck(const QStyleOption *opt, QPainter *p, const QWidget *w, bool radio) const
{
    const Pal c = pal(opt);
    const bool enabled = opt->state & State_Enabled;
    const bool on = opt->state & State_On;
    const bool partial = opt->state & State_NoChange;
    const bool hover = enabled && (opt->state & State_MouseOver);
    const bool pressed = opt->state & State_Sunken;
    const qreal size = radio ? T::Size::Radio : T::Size::Checkbox;
    const QRectF box(QPointF(opt->rect.center()) - QPointF(size / 2 - 0.5, size / 2 - 0.5), QSizeF(size, size));
    const QRectF r = box.adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal hv = anim(opt, w, KeyCheck, hover);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (on || partial) {
        const QColor fill = !enabled ? argb(c.f.accentDisabled) : withAlpha(c.fill, pressed ? 0.8 : 1.0 - 0.1 * hv);
        if (radio) {
            p->setPen(Qt::NoPen);
            p->setBrush(fill);
            p->drawEllipse(box);
            const qreal dot = (pressed ? T::Size::RadioGlyphPressed : T::Size::RadioGlyph + (T::Size::RadioGlyphHover - T::Size::RadioGlyph) * hv) / 2;
            p->setBrush(enabled ? c.onAccent : argb(c.f.textOnAccentDisabled));
            p->drawEllipse(box.center(), dot, dot);
        } else {
            roundRect(p, box, controlRadius(), fill);
            const QColor glyph = enabled ? c.onAccent : argb(c.f.textOnAccentDisabled);
            if (partial) {
                p->setPen(QPen(glyph, 1.5, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(box.left() + size * 0.28, box.center().y()), QPointF(box.right() - size * 0.28, box.center().y()));
            } else {
                checkmark(p, box.adjusted(3, 3, -3, -3), glyph);
            }
        }
    } else {
        const QColor fill = pressed ? argb(c.f.controlAltFillPressed) : mix(argb(c.f.controlAltFill), argb(c.f.controlAltFillHover), hv);
        const QPen pen(argb(enabled ? c.f.controlStrongStroke : c.f.controlStrongStrokeDisabled), 1);
        if (radio) {
            p->setPen(pen);
            p->setBrush(fill);
            p->drawEllipse(r);
            if (pressed) {
                p->setPen(Qt::NoPen);
                p->setBrush(argb(c.f.textSecondary));
                p->drawEllipse(box.center(), T::Size::RadioGlyphPressed / 2, T::Size::RadioGlyphPressed / 2);
            }
        } else {
            roundRect(p, r, controlRadius(), fill, pen);
        }
    }
    p->restore();
    if (keyboardFocus(opt)) {
        focusRing(p, box.toAlignedRect().adjusted(-3, -3, 3, 3), radio ? size / 2 + 3 : controlRadius() + 3, c);
    }
}

void Style::drawMenuItem(const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
    if (!mi) {
        return;
    }
    const Pal c = pal(opt);
    const QRect r = mi->rect;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
        const int pad = int(T::Size::MenuPad);
        p->fillRect(QRect(r.left() - pad, r.center().y(), r.width() + 2 * pad, 1), argb(c.f.divider));
        p->restore();
        return;
    }
    if (mi->menuItemType == QStyleOptionMenuItem::EmptyArea || mi->menuItemType == QStyleOptionMenuItem::TearOff) {
        p->restore();
        return;
    }
    const bool enabled = mi->state & State_Enabled;
    const bool selected = enabled && (mi->state & State_Selected);
    if (selected) {
        roundRect(p, QRectF(r.adjusted(0, 2, 0, -2)), T::Radius::MenuItem, argb((mi->state & State_Sunken) ? c.f.subtleFillPressed : c.f.subtleFillHover));
    }
    const QColor text = enabled ? mi->palette.color(QPalette::WindowText) : argb(c.f.textDisabled);
    const int padX = int(T::Size::MenuItemPadX);
    const int slot = int(T::Size::MenuIconSlot);
    const bool hasSlot = mi->maxIconWidth > 0 || mi->menuHasCheckableItems;
    const QRect iconBox(r.left() + padX, r.top() + (r.height() - int(T::Size::MenuIcon)) / 2, int(T::Size::MenuIcon), int(T::Size::MenuIcon));

    if (mi->checkType != QStyleOptionMenuItem::NotCheckable && mi->checked) {
        if (mi->checkType == QStyleOptionMenuItem::Exclusive) {
            p->setPen(Qt::NoPen);
            p->setBrush(text);
            p->drawEllipse(QRectF(iconBox).center(), 3, 3);
        } else {
            checkmark(p, QRectF(iconBox).adjusted(1, 1, -1, -1), text);
        }
    } else if (!mi->icon.isNull()) {
        const QIcon::Mode mode = !enabled ? QIcon::Disabled : selected ? QIcon::Active : QIcon::Normal;
        mi->icon.paint(p, iconBox, Qt::AlignCenter, mode);
    }

    const int textLeft = r.left() + padX + (hasSlot ? slot : 0);
    const int arrow = mi->menuItemType == QStyleOptionMenuItem::SubMenu ? 16 : 0;
    QRect textRect(textLeft, r.top(), r.right() - padX - arrow - textLeft, r.height());
    QString label = mi->text;
    QString shortcut;
    if (const int tab = label.indexOf(QLatin1Char('\t')); tab >= 0) {
        shortcut = label.mid(tab + 1);
        label = label.left(tab);
    }
    const int mnemonic = styleHint(SH_UnderlineShortcut, mi, w) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
    QFont font = mi->font;
    if (mi->menuItemType == QStyleOptionMenuItem::DefaultItem) {
        font.setWeight(QFont::DemiBold);
    }
    p->setFont(font);
    p->setPen(text);
    p->drawText(visualRect(mi->direction, r, textRect), int(Qt::AlignVCenter | Qt::AlignLeading | Qt::TextSingleLine) | mnemonic, label);
    if (!shortcut.isEmpty()) {
        p->setPen(enabled ? argb(c.f.textTertiary) : argb(c.f.textDisabled));
        p->drawText(visualRect(mi->direction, r, textRect), Qt::AlignVCenter | Qt::AlignTrailing | Qt::TextSingleLine, shortcut);
    }
    if (arrow) {
        const QRectF a(r.right() - padX - 8, r.center().y() - 4, 8, 8);
        chevron(p, visualRect(mi->direction, r, a.toAlignedRect()), mi->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow, text);
    }
    p->restore();
}

void Style::drawTab(const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt);
    if (!tab) {
        return;
    }
    const Pal c = pal(opt);
    const bool selected = tab->state & State_Selected;
    const qreal hv = anim(opt, w, KeyTab ^ quintptr(tab->rect.x() << 8), (tab->state & State_MouseOver) && !selected);
    const qreal radius = tab->documentMode ? T::Radius::Overlay : controlRadius();
    const QRectF r = QRectF(tab->rect).adjusted(1, 2, -1, 0);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const bool north = tab->shape == QTabBar::RoundedNorth || tab->shape == QTabBar::TriangularNorth;
    const bool south = tab->shape == QTabBar::RoundedSouth || tab->shape == QTabBar::TriangularSouth;
    if (north && isTitleTabs(w)) {
        // Windows title-row tab: the selected one grows out of the command area below through concave feet.
        const qreal f = TitleTabFlare;
        const QRectF body = QRectF(tab->rect).adjusted(f, 0, -f, 0);
        if (selected) {
            const qreal top = body.top(), bottom = tab->rect.bottom() + 1, rr = T::Radius::Overlay;
            QPainterPath path;
            path.moveTo(body.left() - f, bottom);
            path.arcTo(QRectF(body.left() - 2 * f, bottom - 2 * f, 2 * f, 2 * f), 270, 90);
            path.arcTo(QRectF(body.left(), top, 2 * rr, 2 * rr), 180, -90);
            path.arcTo(QRectF(body.right() - 2 * rr, top, 2 * rr, 2 * rr), 90, -90);
            path.arcTo(QRectF(body.right(), bottom - 2 * f, 2 * f, 2 * f), 180, 90);
            path.closeSubpath();
            p->fillPath(path, argb(c.f.layerOnMica));
        } else {
            if (hv > 0) {
                roundRect(p, body.adjusted(0, 6, 0, -4), controlRadius(), withAlpha(argb(c.f.subtleFillHover), hv));
            }
            const bool last = tab->position == QStyleOptionTab::End || tab->position == QStyleOptionTab::OnlyOneTab;
            if (!last && tab->selectedPosition != QStyleOptionTab::NextIsSelected) {
                const qreal cy = body.center().y() + 2;
                p->fillRect(QRectF(tab->rect.right(), cy - 8, 1, 16), argb(c.f.divider));
            }
        }
        p->restore();
        return;
    }
    if (isPageTabs(tab, w) && (north || south)) {
        // Page tabs (QTabWidget, Qt Quick TabBar): tab buttons over a transparent page; the selected one is a button plate.
        const QRectF plate = QRectF(tab->rect).adjusted(2.5, 2.5, -2.5, -2.5);
        if (selected) {
            roundRect(p, plate, controlRadius(), argb(c.f.controlFill), QPen(argb(c.f.controlStroke), 1));
        } else if (hv > 0) {
            roundRect(p, plate, controlRadius(), withAlpha(argb(c.f.subtleFillHover), hv));
        }
        p->restore();
        return;
    }
    if (selected) {
        const QColor fill = over(argb(c.f.layerFillAlt), tab->palette.color(QPalette::Window));
        QPainterPath path = north ? roundedPath(r, radius, radius, 0, 0) : south ? roundedPath(r, 0, 0, radius, radius) : roundedPath(r, radius, radius, radius, radius);
        p->setPen(Qt::NoPen);
        p->setBrush(fill);
        p->drawPath(path);
        p->setPen(QPen(argb(c.f.cardStroke), 1));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
        if (north) {
            p->fillRect(QRectF(r.left() + 1, r.bottom() - 1, r.width() - 2, 2), fill);
        }
    } else if (hv > 0) {
        roundRect(p, r.adjusted(2, 2, -2, -4), controlRadius(), withAlpha(argb(c.f.subtleFillHover), hv));
    }
    p->restore();
}

void Style::drawScrollBar(const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const
{
    const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(opt);
    if (!sb) {
        return;
    }
    const Pal c = pal(opt);
    const bool horizontal = sb->orientation == Qt::Horizontal;
    const bool hover = (sb->state & State_MouseOver) && (sb->state & State_Enabled);
    const qreal hv = Config::get().scrollbarExpanded ? 1.0 : anim(opt, w, KeyScroll, hover || (sb->state & State_Sunken), int(T::Motion::FastMs));
    const qreal thickness = T::Size::ScrollbarRest + (T::Size::ScrollbarHover - T::Size::ScrollbarRest) * hv;
    const QRect groove = subControlRect(CC_ScrollBar, sb, SC_ScrollBarGroove, w);
    const QRect slider = subControlRect(CC_ScrollBar, sb, SC_ScrollBarSlider, w);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (hv > 0) {
        roundRect(p, QRectF(groove).adjusted(1, 1, -1, -1), T::Size::Scrollbar / 2, withAlpha(argb(c.f.controlFill), hv));
    }
    if (sb->maximum > sb->minimum) {
        const qreal inset = (T::Size::Scrollbar - thickness) / 2;
        QRectF thumb = horizontal ? QRectF(slider.left() + 2, groove.bottom() + 1 - inset - thickness, slider.width() - 4, thickness)
                                  : QRectF(groove.right() + 1 - inset - thickness, slider.top() + 2, thickness, slider.height() - 4);
        const bool active = (sb->activeSubControls & SC_ScrollBarSlider) && (sb->state & State_Sunken);
        const QColor color = active ? argb(c.f.textSecondary) : argb(c.f.controlStrongFill);
        roundRect(p, thumb, thickness / 2, color);
    }
    p->restore();
}

void Style::drawSlider(const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const
{
    const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(opt);
    if (!sl) {
        return;
    }
    const Pal c = pal(opt);
    const bool enabled = sl->state & State_Enabled;
    const bool horizontal = sl->orientation == Qt::Horizontal;
    const QRect groove = subControlRect(CC_Slider, sl, SC_SliderGroove, w);
    const QRect handle = subControlRect(CC_Slider, sl, SC_SliderHandle, w);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const qreal t = T::Size::SliderTrack;
    roundRect(p, QRectF(groove), t / 2, argb(enabled ? c.f.controlStrongFill : c.f.controlStrongStrokeDisabled));
    QRectF filled;
    const bool reversed = horizontal ? (sl->upsideDown != (sl->direction == Qt::RightToLeft)) : !sl->upsideDown;
    if (horizontal) {
        filled = reversed ? QRectF(handle.center().x(), groove.top(), groove.right() - handle.center().x(), t)
                          : QRectF(groove.left(), groove.top(), handle.center().x() - groove.left(), t);
    } else {
        filled = reversed ? QRectF(groove.left(), handle.center().y(), t, groove.bottom() - handle.center().y())
                          : QRectF(groove.left(), groove.top(), t, handle.center().y() - groove.top());
    }
    roundRect(p, filled, t / 2, enabled ? c.fill : argb(c.f.accentDisabled));

    if (sl->subControls & SC_SliderTickmarks && sl->tickPosition != QSlider::NoTicks) {
        const int interval = sl->tickInterval > 0 ? sl->tickInterval : std::max(1, sl->pageStep);
        const int d = std::lround(T::Size::SliderThumb);
        const int span = (horizontal ? sl->rect.width() : sl->rect.height()) - d;
        const QRectF r(sl->rect);
        p->setPen(QPen(argb(c.f.controlStrongStroke), 1));
        auto tick = [&](int v) {
            const qreal at = (horizontal ? r.left() : r.top()) + sliderPositionFromValue(sl->minimum, sl->maximum, v, span, sl->upsideDown) + d / 2.0;
            if (horizontal) {
                if (sl->tickPosition & QSlider::TicksAbove) {
                    p->drawLine(QPointF(at, r.top()), QPointF(at, r.top() + 4));
                }
                if (sl->tickPosition & QSlider::TicksBelow) {
                    p->drawLine(QPointF(at, r.bottom() - 3), QPointF(at, r.bottom() + 1));
                }
            } else {
                if (sl->tickPosition & QSlider::TicksLeft) {
                    p->drawLine(QPointF(r.left(), at), QPointF(r.left() + 4, at));
                }
                if (sl->tickPosition & QSlider::TicksRight) {
                    p->drawLine(QPointF(r.right() - 3, at), QPointF(r.right() + 1, at));
                }
            }
        };
        int v = sl->minimum;
        for (; v < sl->maximum; v += interval) {
            tick(v);
        }
        tick(sl->maximum); // the last step may be shorter than the interval
    }

    const qreal d = T::Size::SliderThumb;
    const QRectF knob = QRectF(handle).adjusted(0.5, 0.5, -0.5, -0.5);
    const bool hover = enabled && (sl->activeSubControls & SC_SliderHandle) && (sl->state & State_MouseOver);
    const bool pressed = (sl->activeSubControls & SC_SliderHandle) && (sl->state & State_Sunken);
    const qreal hv = anim(opt, w, KeySlider, hover);
    p->setPen(QPen(argb(c.f.controlStrokeSecondary), 1));
    p->setBrush(argb(c.f.controlSolidFill));
    p->drawEllipse(knob);
    const qreal inner = (pressed ? T::Size::SliderThumbInnerPressed : T::Size::SliderThumbInner + (T::Size::SliderThumbInnerHover - T::Size::SliderThumbInner) * hv) / 2;
    p->setPen(Qt::NoPen);
    p->setBrush(enabled ? c.fill : argb(c.f.accentDisabled));
    p->drawEllipse(knob.center(), inner, inner);
    p->restore();
    if (keyboardFocus(opt)) {
        focusRing(p, knob.toAlignedRect().adjusted(-3, -3, 3, 3), d / 2 + 3, c);
    }
}

void Style::drawProgress(const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
    if (!pb) {
        return;
    }
    const Pal c = pal(opt);
    const bool horizontal = pb->state & State_Horizontal;
    const QRect area = subElementRect(SE_ProgressBarGroove, pb, w);
    const qreal h = T::Size::ProgressH;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF track = horizontal ? QRectF(area.left(), area.center().y() - h / 2 + 0.5, area.width(), h)
                                    : QRectF(area.center().x() - h / 2 + 0.5, area.top(), h, area.height());
    const QRectF line = horizontal ? QRectF(track.left(), track.center().y() - 0.5, track.width(), 1) : QRectF(track.center().x() - 0.5, track.top(), 1, track.height());
    p->fillRect(line, argb(c.f.controlStrongStroke));
    const QColor bar = (pb->state & State_Enabled) ? c.fill : argb(c.f.accentDisabled);
    const qint64 range = qint64(pb->maximum) - pb->minimum;
    if (range <= 0) {
        const qreal phase = m_anim->busyPhase(w ? static_cast<const QObject *>(w) : opt->styleObject, 1800);
        const qreal len = horizontal ? track.width() : track.height();
        const qreal seg = len * 0.4;
        const qreal start = -seg + (len + seg) * phase;
        QRectF s = horizontal ? QRectF(track.left() + start, track.top(), seg, h) : QRectF(track.left(), track.top() + start, h, seg);
        p->setClipRect(track);
        roundRect(p, s, h / 2, bar);
    } else {
        const qreal frac = qBound(0.0, qreal(pb->progress - pb->minimum) / range, 1.0);
        QRectF s = track;
        if (horizontal) {
            s.setWidth(track.width() * frac);
            if (pb->invertedAppearance != (pb->direction == Qt::RightToLeft)) {
                s.moveRight(track.right());
            }
        } else {
            s.setHeight(track.height() * frac);
            if (!pb->bottomToTop != pb->invertedAppearance) {
                s.moveTop(track.top());
            } else {
                s.moveBottom(track.bottom());
            }
        }
        roundRect(p, s, h / 2, bar);
    }
    p->restore();
}

// Storage bar (KCapacityBar): thick track like the Windows drive bars, red when nearly full.
void Style::drawCapacity(const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
    if (!pb) {
        return;
    }
    const Pal c = pal(opt);
    const QRect r = opt->rect;
    const bool hasText = pb->textVisible && !pb->text.isEmpty();
    const bool textInside = hasText && w && w->property("drawTextMode").toInt() == 0;
    const qreal h = textInside ? r.height() : std::min<qreal>(T::Size::CapacityH, r.height());
    const qreal top = textInside || !hasText ? r.top() + (r.height() - h) / 2 : r.top();
    const QRectF track(r.left(), top, r.width(), h);
    const qreal radius = std::min<qreal>(h / 2, T::Radius::Control);
    const qint64 range = qint64(pb->maximum) - pb->minimum;
    const qreal frac = range > 0 ? qBound(0.0, qreal(pb->progress - pb->minimum) / range, 1.0) : 0;
    QColor bar = !(pb->state & State_Enabled) ? argb(c.f.accentDisabled) : frac >= 0.9 ? argb(c.f.critical) : c.fill;
    if (textInside) {
        bar.setAlphaF(bar.alphaF() * 0.45);
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    roundRect(p, track, radius, argb(c.f.controlStrokeSecondary));
    if (frac > 0) {
        QRectF fill = track;
        fill.setWidth(std::max(track.width() * frac, 2 * radius));
        if (pb->direction == Qt::RightToLeft) {
            fill.moveRight(track.right());
        }
        roundRect(p, fill, radius, bar);
    }
    if (hasText) {
        const QRect area = textInside ? r.adjusted(6, 0, -6, 0) : r.adjusted(0, int(h) + 2, 0, 0);
        const Qt::Alignment align = w ? w->property("horizontalTextAlignment").value<Qt::Alignment>() : Qt::Alignment(Qt::AlignHCenter);
        p->setPen(textInside ? pb->palette.color(QPalette::WindowText) : argb(c.f.textSecondary));
        p->drawText(area, Qt::AlignVCenter | (align & Qt::AlignHorizontal_Mask), opt->fontMetrics.elidedText(pb->text, Qt::ElideMiddle, area.width()));
    }
    p->restore();
}

void Style::drawItemViewItem(const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    const auto *vi = qstyleoption_cast<const QStyleOptionViewItem *>(opt);
    if (!vi) {
        return;
    }
    const Pal c = pal(opt);
    const bool selected = vi->state & State_Selected;
    const bool hover = (vi->state & State_MouseOver) && (vi->state & State_Enabled);
    if (!selected && !hover) {
        return;
    }
    const auto pos = vi->viewItemPosition;
    const qreal rad = controlRadius();
    const bool first = pos == QStyleOptionViewItem::Beginning || pos == QStyleOptionViewItem::OnlyOne || pos == QStyleOptionViewItem::Invalid;
    const bool last = pos == QStyleOptionViewItem::End || pos == QStyleOptionViewItem::OnlyOne || pos == QStyleOptionViewItem::Invalid;
    QRectF r = QRectF(vi->rect).adjusted(first ? 2 : 0, 1, last ? -2 : 0, -1);
    QColor fill;
    if (selected) {
        const bool active = vi->state & State_Active;
        fill = withAlpha(c.accent, (c.dark ? 0.30 : 0.20) * (active ? 1.0 : 0.75) + (hover ? 0.06 : 0.0));
    } else {
        fill = argb(c.f.subtleFillHover);
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(fill);
    p->drawPath(roundedPath(r, first ? rad : 0, last ? rad : 0, last ? rad : 0, first ? rad : 0));
    if (selected && first && isNavigationList(vi, w) && r.height() >= T::Size::ListPillH + 4) {
        const QRectF pill(r.left(), r.center().y() - T::Size::ListPillH / 2, T::Size::ListPillW, T::Size::ListPillH);
        roundRect(p, pill, T::Radius::ListPill, c.fill);
    }
    p->restore();
}

void Style::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    switch (pe) {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel: {
        const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
        const bool accent = b && (b->features & QStyleOptionButton::DefaultButton);
        const bool flat = b && (b->features & QStyleOptionButton::Flat);
        drawButtonPanel(opt, p, w, accent, flat);
        return;
    }
    case PE_PanelButtonTool: {
        const bool autoRaise = opt->state & State_AutoRaise;
        drawButtonPanel(opt, p, w, false, autoRaise || !(opt->state & State_Raised));
        return;
    }
    case PE_FrameButtonTool:
    case PE_FrameButtonBevel:
    case PE_FrameDefaultButton:
    case PE_FrameMenu:
    case PE_PanelMenuBar:
    case PE_PanelToolBar:
    case PE_IndicatorToolBarHandle:
    case PE_FrameDockWidget:
    case PE_PanelScrollAreaCorner:
    case PE_IndicatorProgressChunk:
    case PE_FrameStatusBarItem:
    case PE_PanelStatusBar:
    case PE_IndicatorButtonDropDown:
    case PE_IndicatorTabTear:
    case PE_IndicatorDockWidgetResizeHandle:
    case PE_FrameTabBarBase:
        return;
    case PE_FrameFocusRect:
        if (keyboardFocus(opt) && !qobject_cast<const QAbstractItemView *>(w)) {
            focusRing(p, opt->rect, controlRadius(), pal(opt));
        }
        return;
    case PE_IndicatorCheckBox:
    case PE_IndicatorItemViewItemCheck:
        drawCheck(opt, p, w, false);
        return;
    case PE_IndicatorRadioButton:
        drawCheck(opt, p, w, true);
        return;
    case PE_IndicatorMenuCheckMark:
        checkmark(p, QRectF(opt->rect).adjusted(2, 2, -2, -2), opt->palette.color(QPalette::WindowText));
        return;
    case PE_PanelLineEdit: {
        const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(opt);
        if (f && f->lineWidth == 0) {
            return;
        }
        if (w && (qobject_cast<const QAbstractSpinBox *>(w->parentWidget()) || qobject_cast<const QComboBox *>(w->parentWidget()))) {
            return;
        }
        drawInputPanel(opt, p, w, opt->state & State_HasFocus);
        return;
    }
    case PE_FrameLineEdit:
        if (w && w->inherits("KUrlNavigator")) {
            // Breadcrumb mode of an address bar: Windows draws a flat, lighter field without the input underline.
            const Pal c = pal(opt);
            const qreal hv = anim(opt, w, KeyInput, opt->state & State_MouseOver);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            panel(p, opt->rect, controlRadius(), mix(argb(c.f.controlFill), argb(c.f.controlFillHover), hv), {argb(c.f.controlStroke)});
            p->restore();
            return;
        }
        drawInputPanel(opt, p, w, opt->state & State_HasFocus);
        return;
    case PE_PanelMenu: {
        const Pal c = pal(opt);
        const bool translucent = translucentSurface(w);
        QColor fill = argb(c.f.bgQuaternary);
        if (translucent) {
            fill.setAlphaF(c.dark ? T::Surface::MenuAlphaDark : T::Surface::MenuAlphaLight);
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const qreal radius = translucent ? T::Radius::Overlay : 0;
        panel(p, opt->rect, radius, fill, {argb(c.f.surfaceStrokeFlyout)});
        if (c.dark) {
            panel(p, opt->rect.adjusted(1, 1, -1, -1), std::max(0.0, radius - 1), Qt::transparent, {QColor(255, 255, 255, 18)});
        }
        p->restore();
        return;
    }
    case PE_PanelTipLabel: {
        const Pal c = pal(opt);
        QColor fill = opt->palette.color(QPalette::ToolTipBase);
        if (translucentSurface(w)) {
            fill.setAlphaF(c.dark ? T::Surface::TooltipAlphaDark : T::Surface::TooltipAlphaLight);
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        panel(p, opt->rect, T::Radius::Tooltip, fill, {argb(c.f.surfaceStrokeFlyout)});
        p->restore();
        return;
    }
    case PE_Frame: {
        const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(opt);
        if (f && f->lineWidth <= 0) {
            return;
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        panel(p, opt->rect, controlRadius(), Qt::transparent, {argb(pal(opt).f.divider)});
        p->restore();
        return;
    }
    case PE_FrameGroupBox: {
        const Pal c = pal(opt);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        panel(p, opt->rect, T::Radius::Overlay, argb(c.f.cardBg), {argb(c.f.cardStroke)});
        p->restore();
        return;
    }
    case PE_FrameTabWidget: {
        if (const auto *tw = qobject_cast<const QTabWidget *>(w); tw && isPageTabs(nullptr, tw->tabBar())) {
            return; // pages sit directly on the window, under tab buttons
        }
        const Pal c = pal(opt);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QColor fill = over(argb(c.f.layerFillAlt), opt->palette.color(QPalette::Window));
        panel(p, opt->rect, T::Radius::Overlay, fill, {argb(c.f.cardStroke)});
        p->restore();
        return;
    }
    case PE_FrameWindow:
        p->setPen(argb(pal(opt).f.surfaceStroke));
        p->drawRect(opt->rect.adjusted(0, 0, -1, -1));
        return;
    case PE_IndicatorToolBarSeparator: {
        const QColor c = argb(pal(opt).f.divider);
        if (opt->state & State_Horizontal) {
            p->fillRect(QRect(opt->rect.center().x(), opt->rect.top() + 6, 1, opt->rect.height() - 12), c);
        } else {
            p->fillRect(QRect(opt->rect.left() + 6, opt->rect.center().y(), opt->rect.width() - 12, 1), c);
        }
        return;
    }
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight:
    case PE_IndicatorSpinUp:
    case PE_IndicatorSpinDown:
    case PE_IndicatorSpinPlus:
    case PE_IndicatorSpinMinus:
    case PE_IndicatorColumnViewArrow: {
        Qt::ArrowType dir = Qt::DownArrow;
        if (pe == PE_IndicatorArrowUp || pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinPlus) {
            dir = Qt::UpArrow;
        } else if (pe == PE_IndicatorArrowLeft) {
            dir = Qt::LeftArrow;
        } else if (pe == PE_IndicatorArrowRight || pe == PE_IndicatorColumnViewArrow) {
            dir = Qt::RightArrow;
        }
        const QColor color = (opt->state & State_Enabled) ? opt->palette.color(QPalette::ButtonText) : argb(pal(opt).f.textDisabled);
        const qreal s = std::min<qreal>(8, std::min(opt->rect.width(), opt->rect.height()));
        // Address bar separators sit beside text: exact centre (QRect::center() rounds up by half a pixel).
        const bool crumb = w && (w->inherits("KDEPrivate::KUrlNavigatorButton") || w->property("_lumen_crumb").toBool());
        const QPointF mid = crumb ? QRectF(opt->rect).center() : QPointF(opt->rect.center());
        chevron(p, QRectF(mid - QPointF(s / 2, s / 2), QSizeF(s, s)), dir, color);
        return;
    }
    case PE_IndicatorBranch: {
        if (!(opt->state & State_Children)) {
            return;
        }
        const QColor color = argb(pal(opt).f.textSecondary);
        const QRectF box(QPointF(opt->rect.center()) - QPointF(4, 4), QSizeF(8, 8));
        const bool rtl = opt->direction == Qt::RightToLeft;
        chevron(p, box, (opt->state & State_Open) ? Qt::DownArrow : (rtl ? Qt::LeftArrow : Qt::RightArrow), color);
        return;
    }
    case PE_IndicatorHeaderArrow: {
        const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(opt);
        if (!h || h->sortIndicator == QStyleOptionHeader::None) {
            return;
        }
        // Explorer: a one-pixel chevron above the label, pointing up for ascending (Qt's SortDown).
        const QRect &r = opt->rect;
        const int cx = r.left() + r.width() / 2;
        const int half = r.width() / 2;
        const bool up = h->sortIndicator == QStyleOptionHeader::SortDown;
        const int tip = up ? r.top() : r.top() + half;
        const int ends = up ? r.top() + half : r.top();
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setPen(QPen(argb(pal(opt).f.headerArrow), 0));
        p->drawLine(cx - half, ends, cx, tip);
        p->drawLine(cx, tip, cx + half, ends);
        p->restore();
        return;
    }
    case PE_IndicatorTabClose: {
        const Pal c = pal(opt);
        const QRectF r = QRectF(opt->rect);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (opt->state & (State_MouseOver | State_Sunken)) {
            roundRect(p, r.adjusted(1, 1, -1, -1), controlRadius(), argb((opt->state & State_Sunken) ? c.f.subtleFillPressed : c.f.subtleFillHover));
        }
        p->restore();
        const QPointF m(std::floor(r.center().x()) + 0.5, std::floor(r.center().y()) + 0.5);
        QPainterPath x;
        x.moveTo(m - QPointF(4, 4));
        x.lineTo(m + QPointF(4, 4));
        x.moveTo(m + QPointF(4, -4));
        x.lineTo(m + QPointF(-4, 4));
        strokeFill(p, x, opt->palette.color(QPalette::WindowText), 1.0, Qt::FlatCap);
        return;
    }
    case PE_PanelItemViewItem:
        drawItemViewItem(opt, p, w);
        return;
    case PE_PanelItemViewRow: {
        const auto *vi = qstyleoption_cast<const QStyleOptionViewItem *>(opt);
        if (vi && (vi->features & QStyleOptionViewItem::Alternate)) {
            p->fillRect(opt->rect, opt->palette.color(QPalette::AlternateBase));
        }
        return;
    }
    case PE_IndicatorItemViewItemDrop: {
        const Pal c = pal(opt);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (opt->rect.height() <= 2) {
            p->fillRect(opt->rect.adjusted(0, -1, 0, 1), c.fill);
        } else {
            roundRect(p, strokeRect(opt->rect), controlRadius(), withAlpha(c.accent, 0.15), QPen(c.fill, 1));
        }
        p->restore();
        return;
    }
    default:
        break;
    }
    QCommonStyle::drawPrimitive(pe, opt, p, w);
}

void Style::drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p, const QWidget *w) const
{
    switch (ce) {
    case CE_PushButtonLabel: {
        const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
        if (!b) {
            break;
        }
        QStyleOptionButton copy(*b);
        if (hidesButtonIcon(b)) {
            copy.icon = QIcon();
        }
        if ((b->features & QStyleOptionButton::DefaultButton) && (b->state & State_Enabled)) {
            copy.palette.setColor(QPalette::ButtonText, pal(opt).onAccent);
        }
        QCommonStyle::drawControl(ce, &copy, p, w);
        return;
    }
    case CE_MenuItem:
        drawMenuItem(opt, p, w);
        return;
    case CE_MenuEmptyArea:
    case CE_MenuBarEmptyArea:
    case CE_MenuTearoff:
    case CE_ToolBar:
    case CE_SizeGrip:
    case CE_HeaderEmptyArea:
    case CE_FocusFrame:
        return;
    case CE_MenuScroller: {
        const Pal c = pal(opt);
        chevron(p, QRectF(QPointF(opt->rect.center()) - QPointF(4, 3), QSizeF(8, 6)), (opt->state & State_DownArrow) ? Qt::DownArrow : Qt::UpArrow, argb(c.f.textSecondary));
        return;
    }
    case CE_MenuBarItem: {
        const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
        if (!mi) {
            break;
        }
        const Pal c = pal(opt);
        const bool active = (mi->state & State_Selected) && (mi->state & State_Enabled);
        if (active) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            roundRect(p, QRectF(mi->rect).adjusted(1, 2, -1, -2), controlRadius(), argb((mi->state & State_Sunken) ? c.f.subtleFillPressed : c.f.subtleFillHover));
            p->restore();
        }
        const int mnemonic = styleHint(SH_UnderlineShortcut, mi, w) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
        drawItemText(p, mi->rect, int(Qt::AlignCenter) | mnemonic, mi->palette, mi->state & State_Enabled, mi->text, QPalette::WindowText);
        return;
    }
    case CE_TabBarTabShape:
        drawTab(opt, p, w);
        return;
    case CE_TabBarTabLabel: {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(opt);
        if (tab && !(tab->state & State_Selected)) {
            QStyleOptionTab copy(*tab);
            copy.palette.setColor(QPalette::WindowText, argb(pal(opt).f.textSecondary));
            QCommonStyle::drawControl(ce, &copy, p, w);
            return;
        }
        break;
    }
    case CE_ProgressBarGroove:
        return;
    case CE_CapacityBar:
        drawCapacity(opt, p, w);
        return;
    case CE_ProgressBarContents:
        drawProgress(opt, p, w);
        return;
    case CE_ProgressBarLabel: {
        const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
        if (pb && pb->textVisible && (pb->state & State_Horizontal)) {
            drawItemText(p, subElementRect(SE_ProgressBarLabel, pb, w), Qt::AlignVCenter | Qt::AlignRight, pb->palette, pb->state & State_Enabled, pb->text,
                         QPalette::WindowText);
        }
        return;
    }
    case CE_HeaderSection: {
        // File Explorer's column header: flat, full-height dividers after each column, no rule below.
        const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(opt);
        const Pal c = pal(opt);
        if (opt->state & (State_MouseOver | State_Sunken)) {
            p->fillRect(opt->rect, argb(c.f.headerHover));
        } else if (!h || h->orientation == Qt::Horizontal) {
            p->fillRect(QRect(opt->rect.right(), opt->rect.top(), 1, opt->rect.height()), argb(c.f.headerDivider));
        } else {
            p->fillRect(QRect(opt->rect.left(), opt->rect.bottom(), opt->rect.width(), 1), argb(c.f.headerDivider));
        }
        return;
    }
    case CE_HeaderLabel: {
        const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(opt);
        if (h) {
            QStyleOptionHeader copy(*h);
            copy.palette.setColor(QPalette::ButtonText, argb(pal(opt).f.headerText));
            if (!(copy.textAlignment & Qt::AlignVertical_Mask)) {
                copy.textAlignment |= Qt::AlignVCenter;
            }
            QCommonStyle::drawControl(ce, &copy, p, w);
            return;
        }
        break;
    }
    case CE_ItemViewItem: {
        const auto *vi = qstyleoption_cast<const QStyleOptionViewItem *>(opt);
        if (vi && (vi->state & State_Selected)) {
            QStyleOptionViewItem copy(*vi);
            copy.palette.setColor(QPalette::HighlightedText, vi->palette.color(QPalette::Text));
            QCommonStyle::drawControl(ce, &copy, p, w);
            return;
        }
        break;
    }
    case CE_Splitter:
        p->fillRect(opt->rect, argb(pal(opt).f.divider));
        return;
    case CE_RubberBand: {
        const Pal c = pal(opt);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        roundRect(p, strokeRect(opt->rect), controlRadius(), withAlpha(c.accent, 0.2), QPen(c.accent, 1));
        p->restore();
        return;
    }
    case CE_ShapedFrame: {
        const auto *f = qstyleoption_cast<const QStyleOptionFrame *>(opt);
        if (f && (f->frameShape == QFrame::HLine || f->frameShape == QFrame::VLine)) {
            const QColor c = argb(pal(opt).f.divider);
            if (f->frameShape == QFrame::HLine) {
                p->fillRect(QRect(opt->rect.left(), opt->rect.center().y(), opt->rect.width(), 1), c);
            } else {
                p->fillRect(QRect(opt->rect.center().x(), opt->rect.top(), 1, opt->rect.height()), c);
            }
            return;
        }
        break;
    }
    case CE_DockWidgetTitle: {
        const auto *dw = qstyleoption_cast<const QStyleOptionDockWidget *>(opt);
        if (!dw) {
            break;
        }
        const QRect r = subElementRect(SE_DockWidgetTitleBarText, opt, w);
        const QString text = opt->fontMetrics.elidedText(dw->title, Qt::ElideRight, r.width());
        drawItemText(p, r, Qt::AlignVCenter | Qt::AlignLeading, dw->palette, dw->state & State_Enabled, text, QPalette::WindowText);
        return;
    }
    default:
        break;
    }
    QCommonStyle::drawControl(ce, opt, p, w);
}

void Style::drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const
{
    switch (cc) {
    case CC_ScrollBar:
        drawScrollBar(opt, p, w);
        return;
    case CC_Slider:
        drawSlider(opt, p, w);
        return;
    case CC_ComboBox: {
        const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(opt);
        if (!cb) {
            break;
        }
        if (cb->frame) {
            if (cb->editable) {
                drawInputPanel(opt, p, w, cb->state & State_HasFocus);
            } else {
                drawButtonPanel(opt, p, w, false, false);
            }
        }
        const QRect arrow = subControlRect(CC_ComboBox, cb, SC_ComboBoxArrow, w);
        const QColor color = (cb->state & State_Enabled) ? cb->palette.color(QPalette::ButtonText) : argb(pal(opt).f.textDisabled);
        chevron(p, QRectF(QPointF(arrow.center()) - QPointF(5, 3), QSizeF(10, 6)), Qt::DownArrow, withAlpha(color, 0.8));
        return;
    }
    case CC_SpinBox: {
        const auto *sp = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);
        if (!sp) {
            break;
        }
        const Pal c = pal(opt);
        if (sp->frame) {
            drawInputPanel(opt, p, w, sp->state & State_HasFocus);
        }
        if (sp->buttonSymbols == QAbstractSpinBox::NoButtons) {
            return;
        }
        for (auto [sc, dir, flag] : {std::tuple{SC_SpinBoxUp, Qt::UpArrow, QAbstractSpinBox::StepUpEnabled}, std::tuple{SC_SpinBoxDown, Qt::DownArrow, QAbstractSpinBox::StepDownEnabled}}) {
            const QRect r = subControlRect(CC_SpinBox, sp, sc, w);
            const bool enabled = (sp->state & State_Enabled) && (sp->stepEnabled & flag);
            if (enabled && (sp->activeSubControls & sc)) {
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                roundRect(p, QRectF(r), controlRadius(), argb((sp->state & State_Sunken) ? c.f.subtleFillPressed : c.f.subtleFillHover));
                p->restore();
            }
            const QColor color = enabled ? sp->palette.color(QPalette::Text) : argb(c.f.textDisabled);
            chevron(p, QRectF(QPointF(r.center()) - QPointF(4, 4), QSizeF(8, 8)), dir, withAlpha(color, 0.8));
        }
        return;
    }
    case CC_GroupBox: {
        const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);
        if (!gb) {
            break;
        }
        if (isSettingsCard(w)) {
            const Pal c = pal(opt);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            panel(p, subControlRect(cc, gb, SC_GroupBoxFrame, w), controlRadius(), argb(c.f.cardBg), {argb(c.f.cardStroke)});
            p->restore();
        }
        if (gb->subControls & SC_GroupBoxCheckBox) {
            QStyleOptionButton box;
            box.QStyleOption::operator=(*gb);
            box.rect = subControlRect(cc, gb, SC_GroupBoxCheckBox, w);
            drawPrimitive(PE_IndicatorCheckBox, &box, p, w);
        }
        if (!gb->text.isEmpty()) {
            p->save();
            p->setFont(groupTitleFont(w));
            const int mnemonic = styleHint(SH_UnderlineShortcut, gb, w) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
            drawItemText(p, subControlRect(cc, gb, SC_GroupBoxLabel, w), int(Qt::AlignLeft | Qt::AlignVCenter) | mnemonic, gb->palette, gb->state & State_Enabled,
                         gb->text, QPalette::WindowText);
            p->restore();
        }
        return;
    }
    case CC_ToolButton: {
        const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
        if (tb && (tb->features & QStyleOptionToolButton::HasMenu) && !(tb->features & QStyleOptionToolButton::MenuButtonPopup)) {
            // Menu buttons as in the Windows command bar: no corner arrow; labelled ones end in a centered chevron.
            QStyleOptionToolButton plain(*tb);
            plain.features &= ~QStyleOptionToolButton::HasMenu;
            if (tb->toolButtonStyle == Qt::ToolButtonIconOnly || tb->text.isEmpty()) {
                QCommonStyle::drawComplexControl(cc, &plain, p, w);
                return;
            }
            constexpr int arrow = 12;
            const bool rtl = tb->direction == Qt::RightToLeft;
            QStyleOptionToolButton panel(plain);
            panel.text.clear();
            panel.icon = QIcon();
            QCommonStyle::drawComplexControl(cc, &panel, p, w);
            QStyleOptionToolButton label(plain);
            label.rect = rtl ? tb->rect.adjusted(arrow, 0, 0, 0) : tb->rect.adjusted(0, 0, -arrow, 0);
            drawControl(CE_ToolButtonLabel, &label, p, w);
            const qreal cx = rtl ? tb->rect.left() + 4 + arrow / 2.0 : tb->rect.right() - 4 - arrow / 2.0;
            const QColor color = (tb->state & State_Enabled) ? tb->palette.color(QPalette::ButtonText) : argb(pal(opt).f.textDisabled);
            chevron(p, QRectF(cx - 4, QRectF(tb->rect).center().y() - 4, 8, 8), Qt::DownArrow, withAlpha(color, 0.8));
            return;
        }
        if (!tb || !(tb->features & QStyleOptionToolButton::MenuButtonPopup)) {
            break;
        }
        // Split button: one panel over both parts, a divider, then label and chevron.
        const QRect button = subControlRect(cc, tb, SC_ToolButton, w);
        const QRect menu = subControlRect(cc, tb, SC_ToolButtonMenu, w);
        const bool autoRaise = tb->state & State_AutoRaise;
        const bool framed = !autoRaise || (tb->state & (State_MouseOver | State_Sunken | State_On));
        if (framed || (tb->state & State_On)) {
            QStyleOption panelOpt(*tb);
            panelOpt.state &= ~State_HasFocus;
            drawButtonPanel(&panelOpt, p, w, false, autoRaise);
        }
        const Pal c = pal(opt);
        if (framed) {
            const int x = tb->direction == Qt::RightToLeft ? menu.right() : menu.left();
            p->fillRect(QRect(x, tb->rect.top() + 6, 1, tb->rect.height() - 12), argb(c.f.divider));
        }
        QStyleOptionToolButton label(*tb);
        label.rect = button;
        drawControl(CE_ToolButtonLabel, &label, p, w);
        const QColor color = (tb->state & State_Enabled) ? tb->palette.color(QPalette::ButtonText) : argb(c.f.textDisabled);
        chevron(p, QRectF(QPointF(menu.center()) - QPointF(4, 4), QSizeF(8, 8)), Qt::DownArrow, withAlpha(color, 0.8));
        if (keyboardFocus(tb)) {
            focusRing(p, tb->rect, controlRadius() + 1, c);
        }
        return;
    }
    default:
        break;
    }
    QCommonStyle::drawComplexControl(cc, opt, p, w);
}
}
