#include "lumenshadowhelper.h"
#include "lumencolor.h"
#include "lumenconfig.h"
#include "lumenshadow.h"

#include <KWindowEffects>

#include <QEvent>
#include <QPainterPath>
#include <QWidget>
#include <QWindow>

namespace Lumen
{
ShadowHelper::ShadowHelper(QObject *parent)
    : QObject(parent)
{
}

const ShadowHelper::Tiles &ShadowHelper::tiles(bool dark, Kind kind)
{
    Tiles &t = m_cache[dark][kind == Kind::Tooltip];
    if (!t.tiles.isEmpty()) {
        return t;
    }
    std::span<const T::Layer> layers = kind == Kind::Tooltip ? (dark ? std::span<const T::Layer>(T::Shadow::TooltipDark) : std::span<const T::Layer>(T::Shadow::TooltipLight))
                                                             : (dark ? std::span<const T::Layer>(T::Shadow::FlyoutDark) : std::span<const T::Layer>(T::Shadow::FlyoutLight));
    const qreal radius = kind == Kind::Tooltip ? T::Radius::Tooltip : T::Radius::Overlay;
    const ShadowImage s = renderShadow(layers, radius);
    const QImage &img = s.image;
    const int cx = s.inner.center().x(), cy = s.inner.center().y();
    const int w = img.width(), h = img.height();
    auto tile = [&](int x, int y, int tw, int th) {
        auto p = KWindowShadowTile::Ptr::create();
        p->setImage(img.copy(x, y, tw, th));
        p->create();
        return p;
    };
    // Order: top-left, top, top-right, right, bottom-right, bottom, bottom-left, left.
    t.tiles = {tile(0, 0, cx, cy),          tile(cx, 0, 1, cy),          tile(cx + 1, 0, w - cx - 1, cy),          tile(cx + 1, cy, w - cx - 1, 1),
               tile(cx + 1, cy + 1, w - cx - 1, h - cy - 1), tile(cx, cy + 1, 1, h - cy - 1), tile(0, cy + 1, cx, h - cy - 1), tile(0, cy, cx, 1)};
    t.padding = s.padding;
    return t;
}

void ShadowHelper::registerWidget(QWidget *w, Kind kind)
{
    if (m_kinds.contains(w)) {
        return;
    }
    m_kinds.insert(w, kind);
    w->installEventFilter(this);
    connect(w, &QObject::destroyed, this, [this, w] {
        m_kinds.remove(w);
        m_shadows.remove(w);
    });
}

void ShadowHelper::unregisterWidget(QWidget *w)
{
    if (!m_kinds.remove(w)) {
        return;
    }
    w->removeEventFilter(this);
    if (auto s = m_shadows.take(w)) {
        s->destroy();
        s->deleteLater();
    }
}

void ShadowHelper::install(QWidget *w)
{
    QWindow *win = w->windowHandle();
    if (!win) {
        return;
    }
    const Kind kind = m_kinds.value(w);
    const Tiles &t = tiles(isDark(w->palette()), kind);
    QPointer<KWindowShadow> &s = m_shadows[w];
    if (!s) {
        s = new KWindowShadow(w);
    } else if (s->isCreated()) {
        s->destroy();
    }
    s->setTopLeftTile(t.tiles[0]);
    s->setTopTile(t.tiles[1]);
    s->setTopRightTile(t.tiles[2]);
    s->setRightTile(t.tiles[3]);
    s->setBottomRightTile(t.tiles[4]);
    s->setBottomTile(t.tiles[5]);
    s->setBottomLeftTile(t.tiles[6]);
    s->setLeftTile(t.tiles[7]);
    s->setPadding(t.padding);
    s->setWindow(win);
    s->create();
}

void ShadowHelper::updateBlur(QWidget *w)
{
    QWindow *win = w->windowHandle();
    if (!win) {
        return;
    }
    if (!Config::get().materials || !Config::get().translucentMenus) {
        KWindowEffects::enableBlurBehind(win, false);
        return;
    }
    const qreal r = m_kinds.value(w) == Kind::Tooltip ? T::Radius::Tooltip : T::Radius::Overlay;
    QPainterPath path;
    path.addRoundedRect(QRectF(w->rect()), r, r);
    KWindowEffects::enableBlurBehind(win, true, QRegion(path.toFillPolygon().toPolygon()));
}

bool ShadowHelper::eventFilter(QObject *o, QEvent *e)
{
    auto *w = static_cast<QWidget *>(o);
    switch (e->type()) {
    case QEvent::Show:
        install(w);
        updateBlur(w);
        break;
    case QEvent::Resize:
    case QEvent::StyleChange:
        if (w->isVisible()) {
            updateBlur(w);
        }
        break;
    case QEvent::PaletteChange:
        if (w->isVisible()) {
            install(w);
        }
        break;
    default:
        break;
    }
    return false;
}
}
