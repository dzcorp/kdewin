#pragma once

#include <QPersistentModelIndex>
#include <QScrollArea>
#include <QTreeView>
#include <QUrl>

class DriveModel;
class PlacesPanel;

/// File Explorer's navigation tree: This PC, unfolding into the devices of the places model, each unfolding into its folders.
class DriveTree : public QTreeView
{
    Q_OBJECT

public:
    DriveTree(PlacesPanel *places, QWidget *parent);

    void setUrl(const QUrl &url);

Q_SIGNALS:
    void urlActivated(const QUrl &url);
    void tabRequested(const QUrl &url);
    void newWindowRequested(const QUrl &url);
    void openInSplitViewRequested(const QUrl &url);
    void errorMessage(const QString &message);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    friend class DriveDelegate;
    static int depth(const QModelIndex &index);
    static QRect chevronRect(const QRect &row, int depth);
    void activate(const QModelIndex &index);
    void updateHeight();
    void restorePc();

    DriveModel *m_model;
    PlacesPanel *m_places;
    QPersistentModelIndex m_pendingActivation; // a device being mounted for a click
    QUrl m_url;
};

/// Navigation pane: the places list, then This PC as a tree, under one scroll bar.
class NavigationPane : public QScrollArea
{
    Q_OBJECT

public:
    explicit NavigationPane(QWidget *parent);

    PlacesPanel *places() const;
    DriveTree *drives() const;

private:
    PlacesPanel *m_places;
    DriveTree *m_drives;
};
