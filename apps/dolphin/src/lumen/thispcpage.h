#pragma once

#include <KConfigWatcher>
#include <KIO/Global>

#include <QAbstractScrollArea>
#include <QHash>
#include <QIcon>
#include <QPersistentModelIndex>
#include <QTimer>
#include <QUrl>

#include <vector>

class KFileItem;

/// File Explorer's This PC: optional user folders, the devices and drives with their free space, network locations.
class ThisPcPage : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit ThisPcPage(QWidget *parent);

    void reload();
    QString statusText() const;

    /// File Explorer goes up from a drive's root to This PC.
    static QUrl upUrl(const QUrl &url);

Q_SIGNALS:
    void itemActivated(const KFileItem &item);
    void tabRequested(const QUrl &url);
    void windowRequested(const QUrl &url);
    void statusTextChanged(const QString &text);
    void errorMessage(const QString &message);
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    enum Kind { Folders, Drives, Network };
    enum class Open { Here, Tab, Window };
    struct Section {
        Kind kind;
        QRect header;
    };
    struct Tile {
        int section = 0;
        QPersistentModelIndex place;
        QUrl url;
        QString name;
        QIcon icon;
        bool capacity = false;
        KIO::filesize_t size = 0;
        KIO::filesize_t free = 0;
        QRect rect; // content coordinates; null while its section is folded
    };

    void rebuild();
    void relayout();
    void querySpace();
    int tileAt(const QPoint &pos) const;
    int headerAt(const QPoint &pos) const;
    void setHover(int tile, int header);
    void select(int tile);
    void ensureVisible(int tile);
    void open(int tile, Open how);
    void open(const QPersistentModelIndex &place, const QUrl &url, Open how);
    void go(const QUrl &url, Open how);
    void paintHeader(QPainter &p, const Section &section) const;
    void paintTile(QPainter &p, int index) const;

    KConfigWatcher::Ptr m_config;
    QTimer m_rebuild;
    std::vector<Section> m_sections;
    std::vector<Tile> m_tiles;
    QHash<QUrl, std::pair<KIO::filesize_t, KIO::filesize_t>> m_space;
    bool m_freeSpace = true;
    int m_hovered = -1;
    int m_hoveredHeader = -1;
    int m_current = -1;
    int m_pressed = -1;
    bool m_selected = false;
    bool m_keyboardFocus = false;
    QPersistentModelIndex m_pending; // a drive being mounted to open it
    Open m_pendingHow = Open::Here;
};
