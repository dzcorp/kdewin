#include "lumenbutton.h"
#include "lumendecoration.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/ScaleHelpers>

#include <QPainter>
#include "lumenglyph.h"

#include <QPainterPath>

namespace Lumen
{
using Type = KDecoration3::DecorationButtonType;

Button::Button(Type type, Decoration *decoration, QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent)
{
    m_hover.setDuration(int(T::Motion::FasterMs));
    m_hover.setStartValue(0.0);
    m_hover.setEndValue(1.0);
    connect(&m_hover, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_hoverValue = v.toReal();
        update();
    });
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, [this](bool hovered) {
        m_hover.stop();
        m_hover.setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        m_hover.start();
    });
    auto *w = decoration->window();
    switch (type) {
    case Type::Maximize:
        setVisible(w->isMaximizeable());
        connect(w, &KDecoration3::DecoratedWindow::maximizeableChanged, this, &DecorationButton::setVisible);
        connect(w, &KDecoration3::DecoratedWindow::maximizedChanged, this, [this] {
            update();
        });
        break;
    case Type::Minimize:
        setVisible(w->isMinimizeable());
        connect(w, &KDecoration3::DecoratedWindow::minimizeableChanged, this, &DecorationButton::setVisible);
        break;
    case Type::Close:
        setVisible(w->isCloseable());
        connect(w, &KDecoration3::DecoratedWindow::closeableChanged, this, &DecorationButton::setVisible);
        break;
    case Type::ContextHelp:
        setVisible(w->providesContextHelp());
        connect(w, &KDecoration3::DecoratedWindow::providesContextHelpChanged, this, &DecorationButton::setVisible);
        break;
    case Type::Shade:
        setVisible(w->isShadeable());
        connect(w, &KDecoration3::DecoratedWindow::shadeableChanged, this, &DecorationButton::setVisible);
        break;
    case Type::ApplicationMenu:
        setVisible(w->hasApplicationMenu());
        connect(w, &KDecoration3::DecoratedWindow::hasApplicationMenuChanged, this, &DecorationButton::setVisible);
        break;
    case Type::Menu:
        connect(w, &KDecoration3::DecoratedWindow::iconChanged, this, [this] {
            update();
        });
        break;
    default:
        break;
    }
    connect(w, &KDecoration3::DecoratedWindow::activeChanged, this, [this] {
        update();
    });
}

Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<Type>(), args.at(1).value<Decoration *>(), parent)
{
}

KDecoration3::DecorationButton *Button::create(Type type, KDecoration3::Decoration *decoration, QObject *parent)
{
    auto *d = qobject_cast<Decoration *>(decoration);
    return d ? new Button(type, d, parent) : nullptr;
}

qreal Button::preferredWidth() const
{
    switch (type()) {
    case Type::Menu:
        return T::Size::TitlebarIcon + 2 * T::Size::TitlebarPad;
    case Type::Spacer:
        return T::Size::TitlebarPad;
    default:
        return T::Size::CaptionButtonW;
    }
}

void Button::paint(QPainter *p, const QRectF &repaintArea)
{
    if (!isVisible() || !geometry().intersects(repaintArea)) {
        return;
    }
    auto *d = static_cast<Decoration *>(decoration());
    const auto &f = d->palette();
    const QRectF r = geometry();
    const bool close = type() == Type::Close;
    const bool active = d->window()->isActive();

    p->save();
    QColor glyph = active ? d->fontColor() : argb(f.textTertiary);
    if (type() != Type::Menu && type() != Type::Spacer) {
        QColor bg;
        if (isPressed()) {
            bg = argb(close ? f.closePressed : f.subtleFillPressed);
        } else if (m_hoverValue > 0) {
            bg = withAlpha(argb(close ? f.closeHover : f.subtleFillHover), m_hoverValue);
        } else if (isChecked() && type() != Type::Maximize) {
            bg = argb(f.subtleFillPressed);
        }
        if (bg.isValid()) {
            p->fillRect(r, bg);
        }
        if (close && (isPressed() || m_hoverValue > 0)) {
            const QColor white = argb(f.closeGlyph);
            glyph = isPressed() ? withAlpha(white, 0.7) : QColor::fromRgbF(glyph.redF() + (white.redF() - glyph.redF()) * m_hoverValue,
                                                                       glyph.greenF() + (white.greenF() - glyph.greenF()) * m_hoverValue,
                                                                       glyph.blueF() + (white.blueF() - glyph.blueF()) * m_hoverValue);
        } else if (isPressed()) {
            glyph = withAlpha(glyph, 0.75);
        }
    }

    const qreal g = T::Size::CaptionGlyph;
    const QRectF box(r.center() - QPointF(g / 2, g / 2), QSizeF(g, g));
    drawGlyph(p, box, glyph);
    p->restore();
}

void Button::drawGlyph(QPainter *p, const QRectF &box, const QColor &color) const
{
    auto *d = static_cast<Decoration *>(decoration());
    if (type() == Type::Menu) {
        const qreal is = T::Size::TitlebarIcon;
        const QRectF ir(geometry().center() - QPointF(is / 2, is / 2), QSizeF(is, is));
        d->window()->icon().paint(p, ir.toAlignedRect());
        return;
    }
    const qreal scale = d->window()->nextScale();
    const int state = (d->window()->isMaximized() ? 1 : 0) | (isChecked() ? 2 : 0);
    if (box != m_shapeBox || scale != m_shapeScale || state != m_shapeState) {
        m_shapeBox = box;
        m_shapeScale = scale;
        m_shapeState = state;
        m_shape = glyphShape(box, scale);
    }
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillPath(m_shape, color);
}

QPainterPath Button::glyphShape(const QRectF &box, qreal scale) const
{
    auto *d = static_cast<Decoration *>(decoration());
    const qreal px = KDecoration3::pixelSize(scale);
    auto s = [scale](qreal v) {
        return KDecoration3::snapToPixelGrid(v, scale);
    };
    // Box edges run through device-pixel centers; diagonals cross on a pixel center so the X has one middle pixel.
    const qreal l = s(box.left()), t = s(box.top()), w = s(box.width());
    const QRectF b(l + px / 2, t + px / 2, w - px, w - px);
    const QPointF c(l + s(w / 2) + px / 2, t + s(w / 2) + px / 2);
    const qreal h = w / 2 - px / 2;

    switch (type()) {
    case Type::Minimize:
        return captionGlyph(Caption::Minimize, box, scale);
    case Type::Maximize:
        return captionGlyph(d->window()->isMaximized() ? Caption::Restore : Caption::Maximize, box, scale);
    case Type::Close:
        return captionGlyph(Caption::Close, box, scale);
    default:
        break;
    }

    QPainterPath path;
    QPainterPath solid;
    switch (type()) {
    case Type::OnAllDesktops: {
        const QRectF pin(c.x() - 2.5, c.y() - 4.5, 5, 5);
        (isChecked() ? solid : path).addEllipse(pin);
        path.moveTo(c.x(), pin.bottom());
        path.lineTo(c.x(), b.bottom());
        break;
    }
    case Type::KeepAbove:
    case Type::KeepBelow: {
        const qreal dy = type() == Type::KeepAbove ? 1 : -1;
        path.moveTo(b.left(), c.y() + 2 * dy);
        path.lineTo(c.x(), c.y() - 3 * dy);
        path.lineTo(b.right(), c.y() + 2 * dy);
        break;
    }
    case Type::Shade:
        path.moveTo(l, b.top() + 1);
        path.lineTo(l + w, b.top() + 1);
        path.moveTo(b.left() + 1, b.bottom() - 1);
        path.lineTo(c.x(), c.y());
        path.lineTo(b.right() - 1, b.bottom() - 1);
        break;
    case Type::ApplicationMenu:
        for (int i = 0; i < 3; ++i) {
            const qreal y = s(b.top() + 1 + i * (b.height() - 2) / 2) + px / 2;
            path.moveTo(l, y);
            path.lineTo(l + w, y);
        }
        break;
    case Type::ContextHelp:
        path.moveTo(b.left() + 1.5, b.top() + 3);
        path.cubicTo(b.left() + 1.5, b.top() - 0.5, b.right() - 1.5, b.top() - 0.5, b.right() - 1.5, b.top() + 3);
        path.cubicTo(b.right() - 1.5, c.y(), c.x(), c.y(), c.x(), b.bottom() - 3);
        solid.addEllipse(QPointF(c.x(), b.bottom()), 0.75, 0.75);
        break;
    case Type::ExcludeFromCapture:
        path.addRoundedRect(b.adjusted(0, 2, 0, -2), 1.5, 1.5);
        path.moveTo(b.topLeft());
        path.lineTo(b.bottomRight());
        break;
    default:
        return {};
    }

    const QPainterPath shape = strokeGlyph(path, scale);
    return solid.isEmpty() ? shape : shape.united(solid);
}
}
