#include "drivetree.h"

#include "dolphinplacesmodelsingleton.h"
#include "lumen/lumenicons.h"
#include "lumen/lumennames.h"
#include "lumencolor.h"
#include "lumenglyph.h"
#include "placespanel.h"
#include "views/draganddrophelper.h"

#include <KFilePlacesModel>
#include <KIO/DropJob>
#include <KIO/Global>
#include <KJob>
#include <KLocalizedString>
#include <KPropertiesDialog>

#include <QCollator>
#include <QContextMenuEvent>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace
{
constexpr int Indent = 8; // File Explorer: per tree level
constexpr int ChevronX = 16; // chevron centre of a top-level row
constexpr int ChevronBox = 12;
constexpr int TextGap = 6;
constexpr int UrlRole = Qt::UserRole + 1;

QStringList subfolders(const QString &path)
{
    QStringList names;
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        names.append(it.fileName());
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), collator);
    return names;
}
}

/// This PC, its devices from the places model, then folders listed on demand and kept current with a file system watcher.
class DriveModel : public QAbstractItemModel
{
public:
    DriveModel(KFilePlacesModel *places, QObject *parent)
        : QAbstractItemModel(parent)
        , m_places(places)
    {
        auto pc = std::make_unique<Node>();
        pc->parent = &m_root;
        pc->listed = true;
        m_pc = pc.get();
        m_root.children.push_back(std::move(pc));
        syncDevices();
        connect(places, &QAbstractItemModel::rowsInserted, this, &DriveModel::syncDevices);
        connect(places, &QAbstractItemModel::rowsRemoved, this, &DriveModel::syncDevices);
        connect(places, &QAbstractItemModel::rowsMoved, this, &DriveModel::syncDevices);
        connect(places, &QAbstractItemModel::modelReset, this, &DriveModel::syncDevices);
        connect(places, &QAbstractItemModel::layoutChanged, this, &DriveModel::syncDevices);
        connect(places, &QAbstractItemModel::dataChanged, this, &DriveModel::syncDevices);
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DriveModel::relist);
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override
    {
        const Node *p = node(parent);
        if (column != 0 || row < 0 || row >= int(p->children.size())) {
            return {};
        }
        return createIndex(row, 0, p->children[row].get());
    }

    QModelIndex parent(const QModelIndex &child) const override
    {
        const Node *n = node(child);
        return n->parent ? indexOf(n->parent) : QModelIndex();
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return int(node(parent)->children.size());
    }

    int columnCount(const QModelIndex & = {}) const override
    {
        return 1;
    }

    bool hasChildren(const QModelIndex &parent = {}) const override
    {
        const Node *n = node(parent);
        if (!parent.isValid() || n->listed) {
            return !n->children.empty();
        }
        if (n->parent == m_pc) {
            return true; // unfolding an unmounted device mounts it
        }
        if (!n->hasSubfolders) {
            n->hasSubfolders = QDirIterator(n->path, QDir::Dirs | QDir::NoDotAndDotDot).hasNext();
        }
        return *n->hasSubfolders;
    }

    bool canFetchMore(const QModelIndex &parent) const override
    {
        return parent.isValid() && !node(parent)->listed && hasChildren(parent);
    }

    void fetchMore(const QModelIndex &parent) override
    {
        Node *n = node(parent);
        if (n->path.isEmpty()) {
            n->listAfterSetup = true;
            m_places->requestSetup(n->place);
        } else {
            list(n);
        }
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        const Node *n = node(index);
        if (!index.isValid()) {
            return {};
        }
        if (n == m_pc) {
            switch (role) {
            case Qt::DisplayRole:
                return ThisPc::title();
            case Qt::DecorationRole:
                if (n->icon.isNull()) {
                    n->icon = Lumen::fullColorIcon(QIcon::fromTheme(QStringLiteral("computer")));
                }
                return n->icon;
            case UrlRole:
                return ThisPc::url();
            default:
                return {};
            }
        }
        const bool device = n->parent == m_pc;
        switch (role) {
        case Qt::DisplayRole:
            return n->name;
        case Qt::DecorationRole:
            if (device) {
                return m_places->data(n->place, Qt::DecorationRole);
            }
            if (n->icon.isNull()) {
                n->icon = QIcon::fromTheme(KIO::iconNameForUrl(QUrl::fromLocalFile(n->path)));
            }
            return n->icon;
        case Qt::ToolTipRole:
            return n->path;
        case UrlRole:
            return n->path.isEmpty() ? m_places->url(n->place) : QUrl::fromLocalFile(n->path);
        default:
            return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }
        const Qt::ItemFlags base = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
        return node(index)->path.isEmpty() ? base : base | Qt::ItemIsDragEnabled;
    }

    QStringList mimeTypes() const override
    {
        return {QStringLiteral("text/uri-list")};
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QList<QUrl> urls;
        for (const QModelIndex &index : indexes) {
            if (const QString path = node(index)->path; !path.isEmpty()) {
                urls.append(QUrl::fromLocalFile(path));
            }
        }
        auto *mime = new QMimeData;
        mime->setUrls(urls);
        return mime;
    }

    Qt::DropActions supportedDragActions() const override
    {
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    }

    QModelIndex placeOf(const QModelIndex &index) const
    {
        const Node *n = node(index);
        return index.isValid() && n->parent == m_pc ? QModelIndex(n->place) : QModelIndex();
    }

    QModelIndex pcIndex() const
    {
        return indexOf(m_pc);
    }

    /// The listed row of a local folder; devices mounted deeper win over the root file system.
    QModelIndex find(const QString &path) const
    {
        const Node *best = nullptr;
        for (const auto &device : m_pc->children) {
            const QString &root = device->path;
            const bool contains = !root.isEmpty() && (path == root || path.startsWith(root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/')));
            if (contains && (!best || root.size() > best->path.size())) {
                best = device.get();
            }
        }
        while (best && best->path != path) {
            const Node *next = nullptr;
            for (const auto &child : best->children) {
                if (path == child->path || path.startsWith(child->path + QLatin1Char('/'))) {
                    next = child.get();
                    break;
                }
            }
            best = next;
        }
        return best ? indexOf(best) : QModelIndex();
    }

private:
    struct Node {
        Node *parent = nullptr;
        QString path; // empty: an unmounted device
        QString name;
        QPersistentModelIndex place;
        std::vector<std::unique_ptr<Node>> children;
        int row = 0;
        bool listed = false;
        bool listAfterSetup = false;
        mutable std::optional<bool> hasSubfolders;
        mutable QIcon icon;
    };

    Node *node(const QModelIndex &index) const
    {
        return index.isValid() ? static_cast<Node *>(index.internalPointer()) : const_cast<Node *>(&m_root);
    }

    QModelIndex indexOf(const Node *n) const
    {
        return n == &m_root ? QModelIndex() : createIndex(n->row, 0, const_cast<Node *>(n));
    }

    static void renumber(Node *n, int from)
    {
        for (int i = from; i < int(n->children.size()); ++i) {
            n->children[i]->row = i;
        }
    }

    std::unique_ptr<Node> makeFolder(Node *parent, const QString &name, int row) const
    {
        auto n = std::make_unique<Node>();
        n->parent = parent;
        n->name = name;
        n->path = QDir(parent->path).filePath(name);
        n->row = row;
        return n;
    }

    void syncDevices()
    {
        struct Device {
            QPersistentModelIndex place;
            QString path;
            QString name;
        };
        std::vector<Device> devices;
        for (int row = 0; row < m_places->rowCount(); ++row) {
            const QModelIndex place = m_places->index(row, 0);
            if (!PlacesPanel::isDriveGroup(m_places->groupType(place)) || m_places->isHidden(place)) {
                continue;
            }
            const QUrl url = m_places->url(place);
            const bool mounted = !m_places->setupNeeded(place) && url.isLocalFile();
            devices.push_back({place, mounted ? url.toLocalFile() : QString(), place.data(Qt::DisplayRole).toString()});
        }
        const bool same = devices.size() == m_pc->children.size()
            && std::equal(devices.begin(), devices.end(), m_pc->children.begin(), [](const Device &d, const std::unique_ptr<Node> &n) {
                   return d.place == n->place;
               });
        if (!same) {
            beginResetModel();
            for (const auto &n : m_pc->children) {
                unwatch(n.get());
            }
            m_pc->children.clear();
            for (const Device &d : devices) {
                auto n = std::make_unique<Node>();
                n->parent = m_pc;
                n->place = d.place;
                n->path = d.path;
                n->name = d.name;
                n->row = int(m_pc->children.size());
                m_pc->children.push_back(std::move(n));
            }
            endResetModel();
            return;
        }
        for (size_t i = 0; i < devices.size(); ++i) {
            Node *n = m_pc->children[i].get();
            const bool moved = n->path != devices[i].path;
            if (!moved && n->name == devices[i].name) {
                continue;
            }
            if (moved) {
                if (!n->children.empty()) {
                    beginRemoveRows(indexOf(n), 0, int(n->children.size()) - 1);
                    unwatch(n);
                    n->children.clear();
                    endRemoveRows();
                } else {
                    unwatch(n);
                }
                n->path = devices[i].path;
            }
            n->name = devices[i].name;
            Q_EMIT dataChanged(indexOf(n), indexOf(n));
            if (moved && n->listAfterSetup && !n->path.isEmpty()) {
                n->listAfterSetup = false;
                list(n);
            }
        }
    }

    void list(Node *n)
    {
        if (n->listed || n->path.isEmpty()) {
            return;
        }
        const QStringList names = subfolders(n->path);
        n->listed = true;
        m_listed.insert(n->path, n);
        m_watcher.addPath(n->path);
        if (names.isEmpty()) {
            Q_EMIT dataChanged(indexOf(n), indexOf(n));
            return;
        }
        beginInsertRows(indexOf(n), 0, int(names.size()) - 1);
        n->children.reserve(names.size());
        for (int i = 0; i < names.size(); ++i) {
            n->children.push_back(makeFolder(n, names[i], i));
        }
        endInsertRows();
    }

    void relist(const QString &path)
    {
        const QList<Node *> nodes = m_listed.values(path);
        if (nodes.isEmpty()) {
            return;
        }
        const QStringList names = subfolders(path);
        const QSet<QString> present(names.begin(), names.end());
        for (Node *n : nodes) {
            const QModelIndex parent = indexOf(n);
            for (int row = int(n->children.size()) - 1; row >= 0; --row) {
                if (present.contains(n->children[row]->name)) {
                    continue;
                }
                beginRemoveRows(parent, row, row);
                unwatch(n->children[row].get());
                n->children.erase(n->children.begin() + row);
                renumber(n, row);
                endRemoveRows();
            }
            // Folders kept their order: whatever does not line up is new.
            for (int i = 0; i < names.size(); ++i) {
                if (i < int(n->children.size()) && n->children[i]->name == names[i]) {
                    continue;
                }
                beginInsertRows(parent, i, i);
                n->children.insert(n->children.begin() + i, makeFolder(n, names[i], i));
                renumber(n, i);
                endInsertRows();
            }
            if (n->children.empty()) {
                Q_EMIT dataChanged(parent, parent);
            }
        }
    }

    void unwatch(Node *n)
    {
        if (n->listed) {
            m_listed.remove(n->path, n);
            if (!m_listed.contains(n->path)) {
                m_watcher.removePath(n->path);
            }
            n->listed = false;
        }
        for (const auto &child : n->children) {
            unwatch(child.get());
        }
    }

    KFilePlacesModel *m_places;
    Node m_root;
    Node *m_pc = nullptr;
    QFileSystemWatcher m_watcher;
    QMultiHash<QString, Node *> m_listed;
};

/// Navigation pane rows: indented by depth, chevron, icon and name.
class DriveDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return {option.rect.width(), PlacesPanel::RowHeight};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto *tree = static_cast<DriveTree *>(parent());
        const QRect row(0, option.rect.top(), tree->viewport()->width(), option.rect.height());
        PlacesPanel::paintRowBackground(painter, option, row, option.state & QStyle::State_MouseOver);
        const int depth = DriveTree::depth(index);
        const QColor ink = option.palette.color(QPalette::Text);
        if (index.model()->hasChildren(index)) {
            const Lumen::Glyph glyph = tree->isExpanded(index) ? Lumen::Glyph::ChevronDown : Lumen::Glyph::ChevronRight;
            const QColor muted = Lumen::argb(Lumen::fluent(option.palette).textSecondary);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->fillPath(Lumen::glyphPath(glyph, DriveTree::chevronRect(row, depth), tree->devicePixelRatioF()), muted);
            painter->restore();
        }
        const int size = tree->iconSize().width();
        const int x = PlacesPanel::IconX + depth * Indent;
        const QRect icon(x, row.top() + (row.height() - size) / 2, size, size);
        index.data(Qt::DecorationRole).value<QIcon>().paint(painter, icon);
        const QRect text(icon.right() + 1 + TextGap, row.top(), row.right() - icon.right() - TextGap - 8, row.height());
        painter->setPen(ink);
        painter->setFont(option.font);
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, text.width()));
    }
};

DriveTree::DriveTree(PlacesPanel *places, QWidget *parent)
    : QTreeView(parent)
    , m_model(new DriveModel(DolphinPlacesModelSingleton::instance().placesModel(), this))
    , m_places(places)
{
    setModel(m_model);
    setItemDelegate(new DriveDelegate(this));
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setIndentation(0);
    setUniformRowHeights(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setAutoExpandDelay(750);
    setIconSize(QSize(16, 16));
    viewport()->setAttribute(Qt::WA_Hover);
    viewport()->setAutoFillBackground(false);

    connect(this, &QTreeView::expanded, this, &DriveTree::updateHeight);
    connect(this, &QTreeView::collapsed, this, &DriveTree::updateHeight);
    // This PC remembers being folded, as the Devices section did.
    const auto remember = [this](const QModelIndex &index) {
        if (index == m_model->pcIndex() && m_places->isGroupFolded(KFilePlacesModel::DevicesType) == isExpanded(index)) {
            m_places->toggleGroup(KFilePlacesModel::DevicesType);
        }
    };
    connect(this, &QTreeView::expanded, this, remember);
    connect(this, &QTreeView::collapsed, this, remember);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DriveTree::restorePc);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &DriveTree::updateHeight);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &DriveTree::updateHeight);
    connect(m_model, &QAbstractItemModel::modelReset, this, &DriveTree::updateHeight);
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft) {
        if (m_pendingActivation.isValid() && topLeft == m_pendingActivation) {
            if (const QUrl url = topLeft.data(UrlRole).toUrl(); url.isLocalFile() && !m_model->placeOf(topLeft).data(KFilePlacesModel::SetupNeededRole).toBool()) {
                m_pendingActivation = QPersistentModelIndex();
                Q_EMIT urlActivated(url);
            }
        }
    });
    restorePc();
}

void DriveTree::restorePc()
{
    setExpanded(m_model->pcIndex(), !m_places->isGroupFolded(KFilePlacesModel::DevicesType));
    updateHeight();
}

int DriveTree::depth(const QModelIndex &index)
{
    int depth = 0;
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent()) {
        ++depth;
    }
    return depth;
}

QRect DriveTree::chevronRect(const QRect &row, int depth)
{
    const int x = ChevronX + depth * Indent;
    return QRect(x - ChevronBox / 2, row.top() + (row.height() - ChevronBox) / 2, ChevronBox, ChevronBox);
}

void DriveTree::updateHeight()
{
    int rows = 0;
    const std::function<void(const QModelIndex &)> count = [&](const QModelIndex &parent) {
        const int n = m_model->rowCount(parent);
        rows += n;
        for (int row = 0; row < n; ++row) {
            if (const QModelIndex child = m_model->index(row, 0, parent); isExpanded(child)) {
                count(child);
            }
        }
    };
    count(QModelIndex());
    setFixedHeight(rows * PlacesPanel::RowHeight);
    setUrl(m_url);
}

void DriveTree::setUrl(const QUrl &url)
{
    m_url = url;
    const QModelIndex index = ThisPc::is(url) ? m_model->pcIndex()
        : url.isLocalFile()                    ? m_model->find(url.adjusted(QUrl::StripTrailingSlash).toLocalFile())
                                               : QModelIndex();
    if (index.isValid()) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        selectionModel()->clear();
    }
}

void DriveTree::activate(const QModelIndex &index)
{
    const QModelIndex place = m_model->placeOf(index);
    if (place.isValid() && place.data(KFilePlacesModel::SetupNeededRole).toBool()) {
        m_pendingActivation = index;
        DolphinPlacesModelSingleton::instance().placesModel()->requestSetup(place);
        return;
    }
    if (const QUrl url = index.data(UrlRole).toUrl(); url.isValid()) {
        Q_EMIT urlActivated(url);
    }
}

void DriveTree::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid() && event->button() == Qt::LeftButton && m_model->hasChildren(index)) {
        const QRect row = visualRect(index);
        if (chevronRect(row, depth(index)).adjusted(-4, -row.height(), 4, row.height()).contains(event->position().toPoint())) {
            setExpanded(index, !isExpanded(index));
            event->accept();
            return;
        }
    }
    QTreeView::mousePressEvent(event);
}

void DriveTree::mouseReleaseEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const bool click = state() != DraggingState && index.isValid() && index == currentIndex();
    QTreeView::mouseReleaseEvent(event);
    if (!click) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        activate(index);
    } else if (event->button() == Qt::MiddleButton) {
        Q_EMIT tabRequested(index.data(UrlRole).toUrl());
    }
}

void DriveTree::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && currentIndex().isValid()) {
        activate(currentIndex());
        return;
    }
    QTreeView::keyPressEvent(event);
}

void DriveTree::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }
    const QUrl url = index.data(UrlRole).toUrl();
    const QModelIndex place = m_model->placeOf(index);
    auto *places = DolphinPlacesModelSingleton::instance().placesModel();

    QMenu menu(this);
    if (url.isValid()) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"), this, [this, url] {
            Q_EMIT tabRequested(url);
        });
        menu.addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open in New Window"), this, [this, url] {
            Q_EMIT newWindowRequested(url);
        });
        menu.addAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Split View"), this, [this, url] {
            Q_EMIT openInSplitViewRequested(url);
        });
    }
    if (place.isValid()) {
        menu.addSeparator();
        if (place.data(KFilePlacesModel::SetupNeededRole).toBool()) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("media-mount")), i18ndc("kio6", "@action:inmenu", "Mount"), this, [places, place] {
                places->requestSetup(place);
            });
        } else if (QAction *teardown = places->teardownActionForIndex(place)) {
            teardown->setParent(&menu);
            connect(teardown, &QAction::triggered, this, [this, place] {
                m_places->requestTearDownOf(place);
            });
            menu.addAction(teardown);
        }
        if (QAction *eject = places->ejectActionForIndex(place)) {
            eject->setParent(&menu);
            connect(eject, &QAction::triggered, this, [places, place] {
                places->requestEject(place);
            });
            menu.addAction(eject);
        }
    }
    if (url.isLocalFile()) {
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action:inmenu", "Properties"), this, [this, url] {
            KPropertiesDialog::showDialog(url, this);
        });
    }
    menu.exec(event->globalPos());
}

void DriveTree::dragMoveEvent(QDragMoveEvent *event)
{
    QTreeView::dragMoveEvent(event);
    const QUrl url = indexAt(event->position().toPoint()).data(UrlRole).toUrl();
    if (!url.isLocalFile()) {
        event->ignore();
        return;
    }
    DragAndDropHelper::updateDropAction(event, url);
    event->accept();
}

void DriveTree::dropEvent(QDropEvent *event)
{
    stopAutoScroll();
    setState(NoState);
    const QUrl url = indexAt(event->position().toPoint()).data(UrlRole).toUrl();
    if (!url.isLocalFile()) {
        event->ignore();
        return;
    }
    if (KIO::DropJob *job = DragAndDropHelper::dropUrls(url, event, this)) {
        connect(job, &KJob::result, this, [this](KJob *job) {
            if (job->error() && job->error() != KIO::ERR_USER_CANCELED) {
                Q_EMIT errorMessage(job->errorString());
            }
        });
    }
    event->acceptProposedAction();
}

namespace
{
/// The line File Explorer draws between the pinned places and This PC.
class PaneDivider : public QWidget
{
public:
    explicit PaneDivider(QWidget *parent)
        : QWidget(parent)
    {
        setFixedHeight(PlacesPanel::RowHeight);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(6, height() / 2, width() - 11, 1), Lumen::argb(Lumen::fluent(palette()).divider));
    }
};
}

NavigationPane::NavigationPane(QWidget *parent)
    : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(this);
    auto *column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_places = new PlacesPanel(content);
    m_places->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_places->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_places->setDrivesInTree(true);
    m_drives = new DriveTree(m_places, content);
    column->addWidget(m_places);
    column->addWidget(new PaneDivider(content));
    column->addWidget(m_drives);
    column->addStretch(1);
    setWidget(content);
    setFocusProxy(m_places);
}

PlacesPanel *NavigationPane::places() const
{
    return m_places;
}

DriveTree *NavigationPane::drives() const
{
    return m_drives;
}
