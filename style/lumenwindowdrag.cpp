#include "lumenwindowdrag.h"

#include <QDialog>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QWindow>

namespace Lumen
{
WindowDrag::WindowDrag(QObject *parent)
    : QObject(parent)
{
}

void WindowDrag::registerWidget(QWidget *w)
{
    if (qobject_cast<QToolBar *>(w) || qobject_cast<QMenuBar *>(w) || qobject_cast<QStatusBar *>(w) || qobject_cast<QTabBar *>(w)
        || qobject_cast<QDialog *>(w) || qobject_cast<QMainWindow *>(w)) {
        w->removeEventFilter(this);
        w->installEventFilter(this);
    }
}

void WindowDrag::unregisterWidget(QWidget *w)
{
    w->removeEventFilter(this);
}

bool WindowDrag::isDraggable(QWidget *w, const QPoint &pos) const
{
    if (!w->window()->windowHandle() || w->window()->isFullScreen()) {
        return false;
    }
    if (w->cursor().shape() != Qt::ArrowCursor) {
        return false;
    }
    if (auto *bar = qobject_cast<QMenuBar *>(w)) {
        return !bar->actionAt(pos);
    }
    if (auto *tabs = qobject_cast<QTabBar *>(w)) {
        return tabs->tabAt(pos) < 0;
    }
    if (auto *tb = qobject_cast<QToolBar *>(w)) {
        return !tb->actionAt(pos) && tb->childAt(pos) == nullptr && !tb->isFloating();
    }
    return w->childAt(pos) == nullptr;
}

bool WindowDrag::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() != QEvent::MouseButtonPress) {
        return false;
    }
    auto *me = static_cast<QMouseEvent *>(e);
    auto *w = static_cast<QWidget *>(o);
    if (me->button() != Qt::LeftButton || me->modifiers() != Qt::NoModifier || !isDraggable(w, me->position().toPoint())) {
        return false;
    }
    return w->window()->windowHandle()->startSystemMove();
}
}
