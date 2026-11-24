#include "lumendecoration.h"
#include "lumenbutton.h"
#include "lumenconfig.h"
#include "lumenshadow.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationShadow>
#include <KDecoration3/ScaleHelpers>
#include <KConfigWatcher>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <array>

K_PLUGIN_FACTORY_WITH_JSON(LumenDecorationFactory, "lumen.json", registerPlugin<Lumen::Decoration>(); registerPlugin<Lumen::Button>();)

namespace Lumen
{
namespace
{
constexpr qreal ResizeMargin = 6;

/// [dark][active] shadows, shared by every decoration in the process; rebuilt when the radius changes.
std::array<std::array<std::shared_ptr<KDecoration3::DecorationShadow>, 2>, 2> s_shadows;
int s_shadowRadius = -1;

// One watcher for every decoration in KWin: the settings module writes lumenrc with Notify.
KConfigWatcher *lumenrcWatcher()
{
    static KConfigWatcher::Ptr watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig));
    return watcher.data();
}

std::tuple<bool, int, int, int, bool, bool> look()
{
    const Config &c = Config::get();
    return {c.translucentTitlebar, c.titlebarOpacity, c.windowRadius, c.titlebarHeight, c.centerTitle, c.materials};
}

std::shared_ptr<KDecoration3::DecorationShadow> makeShadow(bool dark, bool active)
{
    const int radius = Config::get().windowRadius;
    if (radius != s_shadowRadius) {
        s_shadows = {};
        s_shadowRadius = radius;
    }
    auto &slot = s_shadows[dark][active];
    if (slot) {
        return slot;
    }
    std::span<const T::Layer> layers = active ? (dark ? std::span<const T::Layer>(T::Shadow::WindowActiveDark) : std::span<const T::Layer>(T::Shadow::WindowActiveLight))
                                              : (dark ? std::span<const T::Layer>(T::Shadow::WindowInactiveDark) : std::span<const T::Layer>(T::Shadow::WindowInactiveLight));
    const ShadowImage img = renderShadow(layers, radius);
    slot = std::make_shared<KDecoration3::DecorationShadow>();
    slot->setPadding(QMarginsF(img.padding));
    slot->setInnerShadowRect(QRectF(QPointF(img.inner.center()), QSizeF(1, 1)));
    slot->setShadow(img.image);
    return slot;
}
}

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
{
}

Decoration::~Decoration() = default;

qreal Decoration::snap(qreal v) const
{
    return KDecoration3::snapToPixelGrid(v, window()->nextScale());
}

qreal Decoration::titleBarHeight() const
{
    return m_shadowOnly ? 0 : snap(Config::get().titlebarHeight);
}

bool Decoration::isEdgeToEdge() const
{
    const auto *w = window();
    return w->isMaximized() || w->adjacentScreenEdges() != Qt::Edges();
}

void Decoration::updateDark()
{
    m_dark = Lumen::isDark(window()->color(KDecoration3::ColorGroup::Active, KDecoration3::ColorRole::TitleBar));
}

qreal Decoration::titleBarAlpha() const
{
    const Config &cfg = Config::get();
    if (!cfg.translucentTitlebar || !cfg.materials || !settings()->isAlphaChannelSupported()) {
        return 1.0;
    }
    if (window()->isActive()) {
        return cfg.titlebarOpacity / 100.0;
    }
    return isDark() ? T::Surface::TitlebarAlphaInactiveDark : T::Surface::TitlebarAlphaInactiveLight;
}

bool Decoration::isTranslucent() const
{
    return titleBarAlpha() < 1.0;
}

QColor Decoration::titleBarColor() const
{
    const auto *w = window();
    QColor c = w->color(w->isActive() ? KDecoration3::ColorGroup::Active : KDecoration3::ColorGroup::Inactive, KDecoration3::ColorRole::TitleBar);
    c.setAlphaF(titleBarAlpha());
    return c;
}

QColor Decoration::fontColor() const
{
    const auto *w = window();
    if (w->isActive()) {
        return w->color(KDecoration3::ColorGroup::Active, KDecoration3::ColorRole::Foreground);
    }
    return argb(palette().textTertiary);
}

bool Decoration::init()
{
    m_shadowOnly = style() == KDecoration3::Style::Shadow;
    auto *w = window();
    auto s = settings();

    auto relayout = [this] {
        updateLayout();
        update();
    };
    auto restyle = [this] {
        updateShadow();
        updateLayout();
        update();
    };
    connect(w, &KDecoration3::DecoratedWindow::activeChanged, this, restyle);
    connect(w, &KDecoration3::DecoratedWindow::paletteChanged, this, [this, restyle] {
        updateDark();
        restyle();
    });
    connect(w, &KDecoration3::DecoratedWindow::maximizedChanged, this, relayout);
    connect(w, &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, relayout);
    connect(w, &KDecoration3::DecoratedWindow::nextScaleChanged, this, relayout);
    // Width changes come in bursts while resizing: only the title bar geometry depends on it.
    connect(w, &KDecoration3::DecoratedWindow::widthChanged, this, &Decoration::updateTitleBarGeometry);
    connect(w, &KDecoration3::DecoratedWindow::captionChanged, this, [this] {
        update(QRectF(0, 0, size().width(), titleBarHeight()));
    });
    connect(w, &KDecoration3::DecoratedWindow::iconChanged, this, [this] {
        update();
    });
    connect(s.get(), &KDecoration3::DecorationSettings::alphaChannelSupportedChanged, this, restyle);
    connect(s.get(), &KDecoration3::DecorationSettings::fontChanged, this, relayout);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::createButtons);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::createButtons);
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, [restyle] {
        Config::reload();
        restyle();
    });
    // Live tuning without a KWin reconfigure: restyle only when a decoration value actually changed.
    m_look = look();
    connect(lumenrcWatcher(), &KConfigWatcher::configChanged, this, [this, restyle] {
        Config::reload();
        if (const auto now = look(); now != m_look) {
            m_look = now;
            restyle();
        }
    });

    updateDark();
    if (!m_shadowOnly) {
        createButtons();
    }
    updateShadow();
    updateLayout();
    return true;
}

void Decoration::createButtons()
{
    if (m_shadowOnly) {
        return;
    }
    delete m_left;
    delete m_right;
    m_left = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_right = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);
    m_left->setSpacing(0);
    m_right->setSpacing(0);
    updateButtonsGeometry();
}

void Decoration::updateLayout()
{
    const bool edge = isEdgeToEdge();
    const qreal radius = edge ? 0 : Config::get().windowRadius;
    const bool dark = isDark();
    const QColor stroke = window()->isActive() ? argb(fluent(dark).surfaceStroke) : withAlpha(argb(fluent(dark).surfaceStroke), 0.6);

    setState([&](KDecoration3::DecorationState *st) {
        st->setBorders(QMarginsF(0, titleBarHeight(), 0, 0));
        st->setBorderRadius(KDecoration3::BorderRadius(radius));
        st->setBorderOutline(edge ? KDecoration3::BorderOutline()
                                  : KDecoration3::BorderOutline(snap(T::Size::Border), stroke, KDecoration3::BorderRadius(radius)));
    });

    const qreal rm = edge ? 0 : ResizeMargin;
    if (m_shadowOnly) {
        // Client-drawn title rows (KWin rule on the "org.lumen.titlebar" tag) still resize from outside the edges.
        setResizeOnlyBorders(QMarginsF(rm, rm, rm, rm));
        setOpaque(false);
        setBlurRegion(QRegion());
        return;
    }
    setResizeOnlyBorders(QMarginsF(rm, rm, rm, rm));
    updateTitleBarGeometry();
}

void Decoration::updateTitleBarGeometry()
{
    if (m_shadowOnly) {
        return;
    }
    const QRectF bar(0, 0, size().width(), titleBarHeight());
    setTitleBar(bar);
    const bool translucent = isTranslucent();
    setOpaque(!translucent && (isEdgeToEdge() || Config::get().windowRadius == 0));
    setBlurRegion(translucent ? QRegion(bar.toAlignedRect()) : QRegion());
    updateButtonsGeometry();
}

void Decoration::updateButtonsGeometry()
{
    if (!m_left || !m_right) {
        return;
    }
    const qreal h = titleBarHeight();
    auto layout = [h](KDecoration3::DecorationButtonGroup *g) {
        for (auto *b : g->buttons()) {
            b->setGeometry(QRectF(QPointF(0, 0), QSizeF(static_cast<Button *>(b)->preferredWidth(), h)));
        }
    };
    layout(m_left);
    layout(m_right);
    m_left->setPos(QPointF(0, 0));
    m_right->setPos(QPointF(size().width() - m_right->geometry().width(), 0));
}

void Decoration::updateShadow()
{
    setShadow(makeShadow(isDark(), window()->isActive()));
}

void Decoration::paint(QPainter *p, const QRectF &repaintArea)
{
    if (m_shadowOnly) {
        return;
    }
    const qreal h = titleBarHeight();
    const QRectF bar(0, 0, size().width(), h);
    const qreal r = isEdgeToEdge() ? 0 : std::min<qreal>(Config::get().windowRadius, h);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setCompositionMode(QPainter::CompositionMode_Source);
    if (r <= 0) {
        p->fillRect(bar.intersected(repaintArea), titleBarColor());
    } else {
        p->fillRect(bar.intersected(repaintArea), Qt::transparent);
        QPainterPath top;
        top.moveTo(0, h);
        top.arcTo(QRectF(0, 0, 2 * r, 2 * r), 180, -90);
        top.arcTo(QRectF(bar.width() - 2 * r, 0, 2 * r, 2 * r), 90, -90);
        top.lineTo(bar.width(), h);
        top.closeSubpath();
        p->setCompositionMode(QPainter::CompositionMode_SourceOver);
        p->setRenderHint(QPainter::Antialiasing, true);
        p->fillPath(top, titleBarColor());
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setClipPath(top, Qt::IntersectClip);
    }
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);

    m_left->paint(p, repaintArea);
    m_right->paint(p, repaintArea);

    const qreal pad = T::Size::TitlebarPad;
    const qreal x0 = m_left->buttons().isEmpty() ? pad : m_left->geometry().right() + 4;
    const qreal x1 = m_right->geometry().left() - pad;
    if (x1 > x0 && repaintArea.intersects(QRectF(x0, 0, x1 - x0, h))) {
        const QFontMetricsF fm(settings()->font());
        const QString text = fm.elidedText(window()->caption(), Qt::ElideRight, x1 - x0);
        QRectF textRect(x0, 0, x1 - x0, h);
        if (Config::get().centerTitle) {
            // Center on the whole bar, then clamp into the space between the button groups.
            const qreal w = fm.horizontalAdvance(text);
            const qreal left = std::clamp((size().width() - w) / 2, x0, std::max(x0, x1 - w));
            textRect = QRectF(left, 0, w + 1, h);
        }
        p->setFont(settings()->font());
        p->setPen(fontColor());
        p->setRenderHint(QPainter::TextAntialiasing);
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, text);
    }
    p->restore();
}
}

#include "lumendecoration.moc"
