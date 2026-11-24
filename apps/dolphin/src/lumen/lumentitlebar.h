#pragma once

#include <QAbstractButton>
#include <QWidget>

class QTabBar;
class QToolButton;

namespace Lumen
{
enum class Caption;

class CaptionButton : public QAbstractButton
{
    Q_OBJECT
public:
    CaptionButton(Caption kind, QWidget *parent);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Caption m_kind;
};

/// Windows-style title row drawn by the app: tabs, new-tab button, window drag area and caption buttons.
/// KWin keeps only shadow, outline and rounded corners for the window (decoration policy rule on the window tag).
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    TitleBar(QTabBar *tabs, QWidget *parent);

    QToolButton *newTabButton() const;
    QToolButton *sidebarButton() const;
    /// Tags the window's xdg_toplevel so the "org.lumen.titlebar" KWin rule drops the server title bar.
    static void tagWindow(QWidget *window);

    static constexpr int Height = 40;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Tabs dropped on the empty strip go after the last tab.
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void toggleMaximized();
    void forwardDrop(QDropEvent *event);

    QTabBar *m_tabs;
    QToolButton *m_newTab;
    QToolButton *m_sidebar;
    CaptionButton *m_maximize;
};
}
