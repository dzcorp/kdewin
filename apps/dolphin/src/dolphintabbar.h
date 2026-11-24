/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DOLPHIN_TAB_BAR_H
#define DOLPHIN_TAB_BAR_H

#include "lumen/lumentabdrag.h"

#include <QTabBar>

class DolphinTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit DolphinTabBar(QWidget *parent);

Q_SIGNALS:
    void openNewActivatedTab(int index);
    void tabDragMoveEvent(int index, QDragMoveEvent *event);
    void tabDropEvent(int index, QDropEvent *event);
    void tabDetachRequested(int index);
    void tabRenamed(int index, const QString &label);
    void tabDuplicateRequested(int index);
    void tabSplitRequested(int index);
    /// The pointer left the strip while dragging tab `index`, `grab` pixels from its top-left corner.
    void tabDragOutRequested(int index, const QPoint &grab);
    /// A tab dragged from any Dolphin window was dropped before tab `index` (count(): after the last one).
    void tabStateDropped(int index, const QByteArray &state);

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumSizeHint() const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * Opens a context menu for the tab on the \a event position.
     */
    void contextMenuEvent(QContextMenuEvent *event) override;

private Q_SLOTS:
    void slotAutoActivationTimeout();
    void slotTabBarChanged();

private:
    /**
     * If \a index is a valid index (>= 0), store the index and start the timer
     * (if the interval >= 0 ms). If the index is not valid (< 0), stop the timer.
     */
    void updateAutoActivationTimer(const int index);
    /// Insertion index for a dropped tab at `x`.
    int dropIndexAt(int x) const;

private:
    QTimer *m_autoActivationTimer;
    int m_autoActivationIndex;
    int m_tabToBeClosedOnMiddleMouseButtonRelease = -1;
    int m_pressedTab = -1;
    QPoint m_grab;
    int m_dropIndex = -1;
    std::optional<Lumen::TabDrag::Payload> m_dragged;
};

#endif // DOLPHIN_TAB_BAR_H
