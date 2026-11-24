#pragma once

#include <QObject>

class QWidget;

namespace Lumen
{
/// Move windows by dragging empty areas of toolbars, menubars, status bars, tab bars and dialogs.
class WindowDrag : public QObject
{
public:
    explicit WindowDrag(QObject *parent = nullptr);
    void registerWidget(QWidget *w);
    void unregisterWidget(QWidget *w);
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool isDraggable(QWidget *w, const QPoint &pos) const;
};
}
