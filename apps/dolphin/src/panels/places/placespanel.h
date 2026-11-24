/*
 * SPDX-FileCopyrightText: 2008-2012 Peter Penz <peter.penz19@gmail.com>
 * SPDX-FileCopyrightText: 2010 Christian Muehlhaeuser <muesli@gmail.com>
 * SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLACESPANEL_H
#define PLACESPANEL_H

#include "panels/panel.h"

#include <KFilePlacesView>
#include <QUrl>

#include <Solid/SolidNamespace> // Solid::ErrorType

class QTimer;
namespace Solid
{
class StorageAccess;
}

/**
 * @brief Combines bookmarks and mounted devices as list.
 */
class PlacesPanel : public KFilePlacesView
{
    Q_OBJECT

public:
    explicit PlacesPanel(QWidget *parent);
    ~PlacesPanel() override;

    void setUrl(const QUrl &url); // override

    // for compatibility with Panel, actions that are shown
    // on the view's context menu
    QList<QAction *> customContextMenuActions() const;
    void setCustomContextMenuActions(const QList<QAction *> &actions);

    void requestTearDown();
    void proceedWithTearDown();
    /// Dolphin's unmount flow (leave the device first) for a device row of the places model.
    void requestTearDownOf(const QModelIndex &index);

    /// Drives move to the navigation pane's drive tree; the list drops their rows.
    void setDrivesInTree(bool inTree);
    static bool isDriveGroup(int groupType);
    bool isGroupFolded(int groupType) const;
    void toggleGroup(int groupType);
    int sectionHeaderHeight() const;
    /// Windows navigation pane drawing shared with the drive tree.
    static constexpr int RowHeight = 32;
    static constexpr int IconX = 34; // after the tree's chevron column, as in File Explorer
    static void paintSectionHeader(QPainter *p, const QWidget *widget, const QRect &area, const QString &title, bool folded, bool hovered);
    static void paintRowBackground(QPainter *p, const QStyleOptionViewItem &option, const QRect &row, bool hovered);

public Q_SLOTS:
    void readSettings();

Q_SIGNALS:
    void foldedGroupsChanged();
    void errorMessage(const QString &error);
    void storageTearDownRequested(const QString &mountPath);
    void storageTearDownExternallyRequested(const QString &mountPath);
    void storageTearDownSuccessful();
    void openInSplitViewRequested(const QUrl &url);

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    /// Without its own scroll bar (navigation pane) the list is as tall as its rows.
    void updateGeometries() override;
    QAbstractItemDelegate *itemDelegateForIndex(const QModelIndex &index) const override;

private Q_SLOTS:
    void slotConfigureTrash();
    void slotUrlsDropped(const QUrl &dest, QDropEvent *event, QWidget *parent);
    void slotContextMenuAboutToShow(const QModelIndex &index, QMenu *menu);
    void slotTearDownRequested(const QModelIndex &index);
    void slotTearDownRequestedExternally(const QString &udi);
    void slotTearDownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData);
    void slotRowsInserted(const QModelIndex &parent, int first, int last);
    void slotRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);

private:
    friend class PlacesSectionDelegate;

    void connectDeviceSignals(const QModelIndex &idx);

    // Windows navigation pane: titled sections that fold into their header.
    bool isSectionStart(const QModelIndex &index) const;
    bool isFolded(const QModelIndex &index) const;
    bool isDriveRow(const QModelIndex &index) const;
    int sectionAt(const QPoint &pos) const;
    void toggleSection(int row);

    bool m_drivesInTree = false;
    QAbstractItemDelegate *m_sectionDelegate = nullptr;
    QList<int> m_foldedGroups;
    int m_hoveredSection = -1;

    QList<QAction *> m_customContextMenuActions;

    QPersistentModelIndex m_indexToTearDown;

    std::unique_ptr<QAction> m_configureTrashAction;
    std::unique_ptr<QAction> m_openInSplitView;
    QAction *m_lockPanelsAction;
};

#endif // PLACESPANEL_H
