#include "lumenwindowmaterial.h"
#include "lumenconfig.h"

#include <KWindowEffects>

#include <QAbstractScrollArea>
#include <QCoreApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QEvent>
#include <QMainWindow>
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>
#include <QWindow>

namespace Lumen
{
WindowMaterial::WindowMaterial(QObject *parent)
    : QObject(parent)
{
}

// QWidget::setVisible() creates the native window before the style polishes it. A raster window that is not mapped yet
// can be recreated with an alpha channel; X11 clients may already hold its window id, so only on Wayland.
static bool canHaveAlpha(const QWidget *w)
{
    if (!w->testAttribute(Qt::WA_WState_Created)) {
        return true;
    }
    const QWindow *win = w->windowHandle();
    if (!win) {
        return false;
    }
    if (win->format().hasAlpha()) {
        return true;
    }
    return win->surfaceType() == QSurface::RasterSurface && !win->isVisible() && QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

bool WindowMaterial::eligible(const QWidget *w)
{
    if (!w->isWindow() || !canHaveAlpha(w) || w->testAttribute(Qt::WA_TranslucentBackground)
        || w->testAttribute(Qt::WA_NoSystemBackground) || w->windowFlags().testFlag(Qt::FramelessWindowHint)) {
        return false;
    }
    const Qt::WindowType type = w->windowType();
    if (type != Qt::Window && type != Qt::Dialog) {
        return false;
    }
    if (!qobject_cast<const QMainWindow *>(w) && !qobject_cast<const QDialog *>(w)) {
        return false;
    }
    const Config &cfg = Config::get();
    return cfg.translucentWindows && !cfg.opaqueApps.contains(QCoreApplication::applicationName(), Qt::CaseInsensitive);
}

void WindowMaterial::registerWidget(QWidget *w)
{
    if (m_windows.contains(w) || !eligible(w)) {
        return;
    }
    if (QWindow *win = w->windowHandle(); win && w->testAttribute(Qt::WA_WState_Created) && !win->format().hasAlpha()) {
        win->destroy(); // recreated with alpha when shown (see canHaveAlpha)
    }
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->installEventFilter(this);
    m_windows.insert(w);
    connect(w, &QObject::destroyed, this, [this, w] {
        m_windows.remove(w);
    });
}

void WindowMaterial::unregisterWidget(QWidget *w)
{
    if (m_windows.remove(w)) {
        w->removeEventFilter(this);
    }
}

void WindowMaterial::updateBlur(QWidget *w)
{
    if (QWindow *win = w->windowHandle()) {
        KWindowEffects::enableBlurBehind(win, Config::get().materials && Config::get().translucentWindows);
    }
}

// Containers that repaint the window color over the material (stacked pages, scroll area viewports).
void WindowMaterial::clearWindowFills(QWidget *w)
{
    const QColor window = w->palette().color(QPalette::Window);
    const auto children = w->findChildren<QWidget *>();
    for (QWidget *c : children) {
        if (c->autoFillBackground() && c->backgroundRole() == QPalette::Window && c->palette().color(QPalette::Window) == window) {
            c->setAutoFillBackground(false);
        }
        if (auto *area = qobject_cast<QAbstractScrollArea *>(c); area && area->viewport()->backgroundRole() == QPalette::Window) {
            area->viewport()->setAutoFillBackground(false);
        }
    }
}

bool WindowMaterial::eventFilter(QObject *o, QEvent *e)
{
    auto *w = static_cast<QWidget *>(o);
    switch (e->type()) {
    case QEvent::Paint: {
        // Like Windows 11: Mica only while active; inactive windows and missing materials get a solid background.
        QColor fill = w->palette().color(QPalette::Window);
        const Config &cfg = Config::get();
        const bool mica = cfg.materials && cfg.translucentWindows && w->isActiveWindow() && KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
        fill.setAlpha(mica ? cfg.windowOpacity * 255 / 100 : 255);
        QPainter p(w);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(static_cast<QPaintEvent *>(e)->rect(), fill);
        break;
    }
    case QEvent::Show:
        clearWindowFills(w);
        updateBlur(w);
        break;
    case QEvent::StyleChange:
        updateBlur(w);
        w->update();
        break;
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        w->update();
        break;
    default:
        break;
    }
    return false;
}
}
