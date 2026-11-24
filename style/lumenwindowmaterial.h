#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>

class QWidget;

namespace Lumen
{
/// Mica for application windows: top-level windows get a translucent window-color background and ask the
/// compositor to blur behind them. Item views and editors keep their opaque Base color.
class WindowMaterial : public QObject
{
public:
    explicit WindowMaterial(QObject *parent = nullptr);
    void registerWidget(QWidget *w);
    void unregisterWidget(QWidget *w);
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    static bool eligible(const QWidget *w);
    void updateBlur(QWidget *w);
    void clearWindowFills(QWidget *w);

    QSet<QWidget *> m_windows;
};
}
