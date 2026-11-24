#undef TRANSLATION_DOMAIN
#define TRANSLATION_DOMAIN "lumen_dolphin"

#include "thispcpage.h"

#include "dolphinplacesmodelsingleton.h"
#include "lumen/lumenicons.h"
#include "lumen/lumennames.h"
#include "lumencolor.h"
#include "lumenconfig.h"
#include "lumenglyph.h"
#include "panels/places/placespanel.h"
#include "views/draganddrophelper.h"

#include <KConfigGroup>
#include <KFileItem>
#include <KFilePlacesModel>
#include <KIO/DropJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KLocalizedString>
#include <KPropertiesDialog>
#include <KSharedConfig>

#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QStandardPaths>
#include <QStyleOptionFocusRect>

#include <sys/stat.h>

namespace
{
// File Explorer's This PC at 100 %: 250 × 52 tiles, 48 px icons, a 189 × 13 capacity bar.
constexpr int Left = 14;
constexpr int Top = 4;
constexpr int HeaderH = 28;
constexpr int HeaderGap = 2;
constexpr int SectionGap = 12;
constexpr int ChevronX = 11;
constexpr int ChevronBox = 12;
constexpr int HeaderTextX = 22;
constexpr int TileW = 250;
constexpr int TileH = 52;
constexpr int PitchX = 261;
constexpr int RowGap = 8;
constexpr int IconInset = 2;
constexpr int IconSize = 48;
constexpr int TextX = 56;
constexpr int NameCapTop = 5;
constexpr int SoloCapCentre = 17; // a lone name sits where the two-line text block would start
constexpr int BarY = 19;
constexpr int BarW = 189;
constexpr int BarH = 13;
constexpr int FreeCapTop = 36;

KFilePlacesModel *places()
{
    return DolphinPlacesModelSingleton::instance().placesModel();
}

QSet<int> &folded()
{
    static QSet<int> sections;
    return sections;
}

QString title(int kind)
{
    switch (kind) {
    case 0:
        return i18nc("@title:group This PC section", "Folders");
    case 1:
        return i18nc("@title:group This PC section", "Devices and drives");
    default:
        return i18nc("@title:group This PC section", "Network locations");
    }
}
}

ThisPcPage::ThisPcPage(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_config(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig)))
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setMouseTracking(true);
    viewport()->setAcceptDrops(true);
    verticalScrollBar()->setSingleStep(TileH / 2);

    m_rebuild.setSingleShot(true);
    connect(&m_rebuild, &QTimer::timeout, this, &ThisPcPage::rebuild);
    const auto later = [this] {
        m_rebuild.start();
    };
    KFilePlacesModel *model = places();
    connect(model, &QAbstractItemModel::rowsInserted, this, later);
    connect(model, &QAbstractItemModel::rowsRemoved, this, later);
    connect(model, &QAbstractItemModel::rowsMoved, this, later);
    connect(model, &QAbstractItemModel::modelReset, this, later);
    connect(model, &QAbstractItemModel::layoutChanged, this, later);
    connect(model, &QAbstractItemModel::dataChanged, this, later);
    connect(model, &KFilePlacesModel::setupDone, this, [this](const QModelIndex &index, bool success) {
        if (m_pending.isValid() && index == m_pending) {
            m_pending = QPersistentModelIndex();
            if (success) {
                go(places()->url(index), m_pendingHow);
            }
        }
    });
    connect(m_config.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Files")) {
            m_rebuild.start();
        }
    });
    rebuild();
}

QUrl ThisPcPage::upUrl(const QUrl &url)
{
    if (ThisPc::is(url)) {
        return url;
    }
    const QUrl dir = url.adjusted(QUrl::StripTrailingSlash);
    KFilePlacesModel *model = places();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex place = model->index(row, 0);
        if (PlacesPanel::isDriveGroup(model->groupType(place)) && model->url(place).adjusted(QUrl::StripTrailingSlash) == dir) {
            return ThisPc::url();
        }
    }
    return KIO::upUrl(url);
}

void ThisPcPage::reload()
{
    rebuild();
}

QString ThisPcPage::statusText() const
{
    const QString items = i18ncp("@info:status", "1 item", "%1 items", int(m_tiles.size()));
    return m_selected ? items + QStringLiteral("   |   ") + i18nc("@info:status", "1 item selected") : items;
}

void ThisPcPage::rebuild()
{
    const QUrl current = m_current >= 0 ? m_tiles[m_current].url : QUrl();
    const KConfigGroup files = m_config->config()->group(QStringLiteral("Files"));
    m_freeSpace = files.readEntry("ThisPcFreeSpace", true);
    const bool hidden = files.readEntry("ThisPcHiddenDrives", false);
    KFilePlacesModel *model = places();

    m_sections.clear();
    m_tiles.clear();
    const auto add = [this](Kind kind) {
        m_sections.push_back({kind, QRect()});
        return int(m_sections.size()) - 1;
    };
    const auto tile = [this](int section, const QModelIndex &place, const QUrl &url, const QString &name, const QIcon &icon) -> Tile & {
        Tile t;
        t.section = section;
        t.place = place;
        t.url = url;
        t.name = name;
        t.icon = icon;
        if (const auto space = m_space.constFind(url); space != m_space.cend()) {
            t.size = space->first;
            t.free = space->second;
        }
        return m_tiles.emplace_back(std::move(t));
    };

    if (files.readEntry("ThisPcFolders", false)) {
        const int section = add(Folders);
        for (const auto location : {QStandardPaths::DesktopLocation,
                                    QStandardPaths::DocumentsLocation,
                                    QStandardPaths::DownloadLocation,
                                    QStandardPaths::MusicLocation,
                                    QStandardPaths::PicturesLocation,
                                    QStandardPaths::MoviesLocation}) {
            const QString path = QStandardPaths::writableLocation(location);
            if (path.isEmpty() || QDir(path) == QDir::home() || !QFileInfo(path).isDir()) {
                continue;
            }
            const QUrl url = QUrl::fromLocalFile(path);
            const QModelIndex place = model->closestItem(url);
            if (place.isValid() && model->url(place).matches(url, QUrl::StripTrailingSlash)) {
                tile(section, QModelIndex(), url, model->text(place), place.data(Qt::DecorationRole).value<QIcon>());
            } else {
                tile(section, QModelIndex(), url, QFileInfo(path).fileName(), Lumen::fullColorIcon(QIcon::fromTheme(KIO::iconNameForUrl(url))));
            }
        }
        if (m_tiles.empty()) {
            m_sections.pop_back();
        }
    }

    const auto collect = [&](int section, auto accept) {
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex place = model->index(row, 0);
            if (!accept(model->groupType(place)) || (!hidden && model->isHidden(place))) {
                continue;
            }
            Tile &t = tile(section, place, model->url(place), model->text(place), place.data(Qt::DecorationRole).value<QIcon>());
            t.capacity = place.data(KFilePlacesModel::CapacityBarRecommendedRole).toBool() && t.url.isLocalFile();
        }
    };
    collect(add(Drives), PlacesPanel::isDriveGroup);
    if (files.readEntry("ThisPcNetwork", true)) {
        const size_t before = m_tiles.size();
        collect(add(Network), [](int group) {
            return group == KFilePlacesModel::RemoteType;
        });
        // The network browser itself is the navigation pane's Network, not a location.
        std::erase_if(m_tiles, [](const Tile &t) {
            return t.url.scheme() == QLatin1String("remote") && (t.url.path().isEmpty() || t.url.path() == QLatin1String("/"));
        });
        if (m_tiles.size() == before) {
            m_sections.pop_back();
        }
    }

    m_current = -1;
    for (size_t i = 0; current.isValid() && i < m_tiles.size(); ++i) {
        if (m_tiles[i].url == current) {
            m_current = int(i);
        }
    }
    m_selected = m_selected && m_current >= 0;
    m_hovered = m_hoveredHeader = -1;
    relayout();
    querySpace();
    Q_EMIT statusTextChanged(statusText());
}

void ThisPcPage::relayout()
{
    const int width = viewport()->width();
    const int columns = std::max(1, (width - Left - TileW) / PitchX + 1);
    int y = Top;
    for (size_t s = 0; s < m_sections.size(); ++s) {
        m_sections[s].header = QRect(4, y, std::max(0, width - 8), HeaderH);
        y += HeaderH + HeaderGap;
        const bool shut = folded().contains(m_sections[s].kind);
        int column = 0;
        int rowTop = y;
        bool any = false;
        for (Tile &t : m_tiles) {
            if (t.section != int(s)) {
                continue;
            }
            if (shut) {
                t.rect = QRect();
                continue;
            }
            t.rect = QRect(Left + column * PitchX, rowTop, TileW, TileH);
            any = true;
            if (++column == columns) {
                column = 0;
                rowTop += TileH + RowGap;
            }
        }
        if (any) {
            y = column == 0 ? rowTop - RowGap : rowTop + TileH;
        }
        y += SectionGap;
    }
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, std::max(0, y - viewport()->height()));
    viewport()->update();
}

void ThisPcPage::querySpace()
{
    if (!m_freeSpace || !isVisible()) {
        return;
    }
    for (const Tile &t : m_tiles) {
        if (!t.capacity) {
            continue;
        }
        KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(t.url);
        connect(job, &KJob::result, this, [this, job, url = t.url] {
            if (job->error() || job->size() == 0) {
                return;
            }
            m_space.insert(url, {job->size(), job->availableSize()});
            for (Tile &t : m_tiles) {
                if (t.url == url) {
                    t.size = job->size();
                    t.free = job->availableSize();
                    viewport()->update(t.rect.translated(0, -verticalScrollBar()->value()));
                }
            }
        });
    }
}

int ThisPcPage::tileAt(const QPoint &pos) const
{
    const QPoint p = pos + QPoint(0, verticalScrollBar()->value());
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles[i].rect.contains(p)) {
            return int(i);
        }
    }
    return -1;
}

int ThisPcPage::headerAt(const QPoint &pos) const
{
    const QPoint p = pos + QPoint(0, verticalScrollBar()->value());
    for (size_t s = 0; s < m_sections.size(); ++s) {
        if (m_sections[s].header.contains(p)) {
            return int(s);
        }
    }
    return -1;
}

void ThisPcPage::setHover(int tile, int header)
{
    if (tile != m_hovered || header != m_hoveredHeader) {
        m_hovered = tile;
        m_hoveredHeader = header;
        viewport()->update();
    }
}

void ThisPcPage::select(int tile)
{
    const bool selected = tile >= 0;
    if (tile == m_current && selected == m_selected) {
        return;
    }
    if (selected) {
        m_current = tile;
    }
    m_selected = selected;
    viewport()->update();
    Q_EMIT statusTextChanged(statusText());
}

void ThisPcPage::ensureVisible(int tile)
{
    const QRect r = m_tiles[tile].rect;
    QScrollBar *bar = verticalScrollBar();
    if (r.top() - Top < bar->value()) {
        bar->setValue(r.top() - Top);
    } else if (r.bottom() + Top > bar->value() + viewport()->height()) {
        bar->setValue(r.bottom() + Top - viewport()->height());
    }
}

void ThisPcPage::open(const QPersistentModelIndex &place, const QUrl &url, Open how)
{
    if (place.isValid() && places()->setupNeeded(place)) {
        m_pending = place;
        m_pendingHow = how;
        places()->requestSetup(place);
        return;
    }
    go(url, how);
}

void ThisPcPage::open(int tile, Open how)
{
    open(m_tiles[tile].place, m_tiles[tile].url, how);
}

void ThisPcPage::go(const QUrl &url, Open how)
{
    if (!url.isValid()) {
        return;
    }
    switch (how) {
    case Open::Here:
        Q_EMIT itemActivated(KFileItem(url, QStringLiteral("inode/directory"), S_IFDIR));
        break;
    case Open::Tab:
        Q_EMIT tabRequested(url);
        break;
    case Open::Window:
        Q_EMIT windowRequested(url);
        break;
    }
}

void ThisPcPage::paintEvent(QPaintEvent *event)
{
    QPainter p(viewport());
    const int dy = verticalScrollBar()->value();
    p.translate(0, -dy);
    const QRect area = event->rect().translated(0, dy);
    for (const Section &section : m_sections) {
        if (section.header.intersects(area)) {
            paintHeader(p, section);
        }
    }
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles[i].rect.intersects(area)) {
            paintTile(p, int(i));
        }
    }
}

void ThisPcPage::paintHeader(QPainter &p, const Section &section) const
{
    const auto &f = Lumen::fluent(palette());
    const QRect &r = section.header;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (m_hoveredHeader >= 0 && &m_sections[m_hoveredHeader] == &section) {
        const qreal radius = Lumen::Config::get().controlRadius;
        p.setPen(Qt::NoPen);
        p.setBrush(Lumen::argb(f.subtleFillHover));
        p.drawRoundedRect(QRectF(r), radius, radius);
    }
    const bool shut = folded().contains(section.kind);
    const QRectF chevron(ChevronX - ChevronBox / 2.0, r.center().y() + 1 - ChevronBox / 2.0, ChevronBox, ChevronBox);
    p.fillPath(Lumen::glyphPath(shut ? Lumen::Glyph::ChevronRight : Lumen::Glyph::ChevronDown, chevron, devicePixelRatioF()), Lumen::argb(f.textSecondary));
    QFont font = p.font();
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * 1.25);
    } else {
        font.setPixelSize(qRound(font.pixelSize() * 1.25));
    }
    const QFontMetrics metrics(font);
    p.setFont(font);
    p.setPen(Lumen::argb(f.textPrimary));
    const int baseline = r.top() + (r.height() + metrics.capHeight()) / 2;
    p.drawText(HeaderTextX, baseline, metrics.elidedText(title(section.kind), Qt::ElideRight, std::max(0, r.right() - HeaderTextX - 8)));
    p.restore();
}

void ThisPcPage::paintTile(QPainter &p, int index) const
{
    const Tile &t = m_tiles[index];
    const auto &f = Lumen::fluent(palette());
    std::uint32_t fill = 0;
    if (m_selected && index == m_current) {
        fill = hasFocus() ? f.listSelected : f.listSelectedInactive;
    } else if (index == m_hovered) {
        fill = f.listHover;
    }
    if (fill) {
        p.fillRect(t.rect, Lumen::argb(fill));
    }
    if (index == m_current && hasFocus() && m_keyboardFocus) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = t.rect;
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &p, this);
    }
    t.icon.paint(&p, QRect(t.rect.left() + IconInset, t.rect.top() + IconInset, IconSize, IconSize));

    const QFontMetrics metrics = fontMetrics();
    const int cap = metrics.capHeight();
    const int x = t.rect.left() + TextX;
    const int width = t.rect.right() - x - 4;
    const bool bar = m_freeSpace && t.capacity && t.size > 0;
    p.setPen(Lumen::argb(f.textPrimary));
    const int nameBaseline = t.rect.top() + (bar ? NameCapTop + cap : SoloCapCentre + cap / 2);
    p.drawText(x, nameBaseline, metrics.elidedText(t.name, Qt::ElideRight, width));
    if (!bar) {
        return;
    }
    // Windows keeps the bar light in both themes; nearly full drives turn red.
    const QRect outline(x, t.rect.top() + BarY, BarW, BarH);
    const QRect track = outline.adjusted(1, 1, -1, -1);
    p.fillRect(outline, QColor(0xbc, 0xbc, 0xbc));
    p.fillRect(track, QColor(0xe6, 0xe6, 0xe6));
    const KIO::filesize_t used = t.size - std::min(t.free, t.size);
    const int filled = int(track.width() * double(used) / double(t.size) + 0.5);
    const bool full = t.free * 10 < t.size;
    p.fillRect(QRect(track.left(), track.top(), filled, track.height()), full ? QColor(0xda, 0x26, 0x26) : palette().color(QPalette::Highlight));
    p.setPen(Lumen::argb(f.textSecondary));
    const QString space = i18nc("@info free and total space of a drive", "%1 free of %2", KIO::convertSize(t.free), KIO::convertSize(t.size));
    p.drawText(x, t.rect.top() + FreeCapTop + cap, metrics.elidedText(space, Qt::ElideRight, width));
}

void ThisPcPage::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    relayout();
}

void ThisPcPage::showEvent(QShowEvent *event)
{
    QAbstractScrollArea::showEvent(event);
    querySpace();
}

void ThisPcPage::mousePressEvent(QMouseEvent *event)
{
    m_keyboardFocus = false;
    const QPoint pos = event->position().toPoint();
    m_pressed = tileAt(pos);
    if (event->button() == Qt::LeftButton) {
        if (const int header = headerAt(pos); header >= 0) {
            const int kind = m_sections[header].kind;
            if (!folded().remove(kind)) {
                folded().insert(kind);
            }
            relayout();
            return;
        }
    }
    if (event->button() != Qt::MiddleButton) {
        select(m_pressed);
    }
}

void ThisPcPage::mouseReleaseEvent(QMouseEvent *event)
{
    const int tile = tileAt(event->position().toPoint());
    if (tile < 0 || tile != m_pressed) {
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        open(tile, Open::Tab);
    } else if (event->button() == Qt::LeftButton && style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, this)
               && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
        open(tile, Open::Here);
    }
}

void ThisPcPage::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int tile = tileAt(event->position().toPoint());
    if (tile >= 0 && event->button() == Qt::LeftButton && !style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, this)) {
        open(tile, Open::Here);
    }
}

void ThisPcPage::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const int tile = tileAt(pos);
    setHover(tile, tile < 0 ? headerAt(pos) : -1);
}

bool ThisPcPage::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Leave || event->type() == QEvent::DragLeave) {
        setHover(-1, -1);
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void ThisPcPage::keyPressEvent(QKeyEvent *event)
{
    std::vector<int> order;
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        if (!m_tiles[i].rect.isNull()) {
            order.push_back(int(i));
        }
    }
    if (order.empty()) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    const auto at = std::find(order.begin(), order.end(), m_current);
    const int pos = at == order.end() ? -1 : int(at - order.begin());
    int next = -1;
    switch (event->key()) {
    case Qt::Key_Right:
        next = order[std::min<int>(pos + 1, order.size() - 1)];
        break;
    case Qt::Key_Left:
        next = order[std::max(pos - 1, 0)];
        break;
    case Qt::Key_Home:
        next = order.front();
        break;
    case Qt::Key_End:
        next = order.back();
        break;
    case Qt::Key_Up:
    case Qt::Key_Down: {
        if (pos < 0) {
            next = order.front();
            break;
        }
        // The nearest tile of the closest row above or below.
        const QRect from = m_tiles[m_current].rect;
        const bool down = event->key() == Qt::Key_Down;
        int best = m_current;
        int bestRow = 0;
        int bestDx = 0;
        for (const int i : order) {
            const QRect r = m_tiles[i].rect;
            const int dy = down ? r.top() - from.top() : from.top() - r.top();
            if (dy <= 0) {
                continue;
            }
            const int dx = std::abs(r.center().x() - from.center().x());
            if (best == m_current || dy < bestRow || (dy == bestRow && dx < bestDx)) {
                best = i;
                bestRow = dy;
                bestDx = dx;
            }
        }
        next = best;
        break;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_selected && m_current >= 0) {
            open(m_current, event->modifiers() & Qt::ControlModifier ? Open::Tab : Open::Here);
        }
        return;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    m_keyboardFocus = true;
    select(next);
    ensureVisible(next);
    viewport()->update();
}

void ThisPcPage::contextMenuEvent(QContextMenuEvent *event)
{
    const bool keyboard = event->reason() == QContextMenuEvent::Keyboard;
    const int tile = keyboard ? (m_selected ? m_current : -1) : tileAt(event->pos());
    if (tile < 0) {
        return;
    }
    select(tile);
    const Tile &t = m_tiles[tile];
    const QPersistentModelIndex place = t.place;
    const QUrl url = t.url;
    KFilePlacesModel *model = places();

    QMenu menu(this);
    QAction *openHere = menu.addAction(i18nc("@action:inmenu", "Open"), this, [this, place, url] {
        open(place, url, Open::Here);
    });
    menu.setDefaultAction(openHere);
    menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"), this, [this, place, url] {
        open(place, url, Open::Tab);
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open in New Window"), this, [this, place, url] {
        open(place, url, Open::Window);
    });
    if (place.isValid() && model->isDevice(place)) {
        menu.addSeparator();
        if (model->setupNeeded(place)) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("media-mount")), i18ndc("kio6", "@action:inmenu", "Mount"), this, [model, place] {
                model->requestSetup(place);
            });
        } else if (QAction *teardown = model->teardownActionForIndex(place)) {
            teardown->setParent(&menu);
            connect(teardown, &QAction::triggered, this, [model, place] {
                model->requestTeardown(place);
            });
            menu.addAction(teardown);
        }
        if (QAction *eject = model->ejectActionForIndex(place)) {
            eject->setParent(&menu);
            connect(eject, &QAction::triggered, this, [model, place] {
                model->requestEject(place);
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
    const QPoint global = keyboard ? viewport()->mapToGlobal(t.rect.center() - QPoint(0, verticalScrollBar()->value())) : event->globalPos();
    menu.exec(global);
}

void ThisPcPage::focusInEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusInEvent(event);
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    if (m_current < 0 && !m_tiles.empty()) {
        m_current = 0;
    }
    viewport()->update();
    Q_EMIT activated();
}

void ThisPcPage::focusOutEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusOutEvent(event);
    viewport()->update();
}

void ThisPcPage::dragEnterEvent(QDragEnterEvent *event)
{
    event->setAccepted(event->mimeData()->hasUrls());
}

void ThisPcPage::dragMoveEvent(QDragMoveEvent *event)
{
    const int tile = tileAt(event->position().toPoint());
    setHover(tile, -1);
    if (tile < 0 || !m_tiles[tile].url.isLocalFile()) {
        event->ignore();
        return;
    }
    DragAndDropHelper::updateDropAction(event, m_tiles[tile].url);
    event->accept();
}

void ThisPcPage::dropEvent(QDropEvent *event)
{
    const int tile = tileAt(event->position().toPoint());
    setHover(-1, -1);
    if (tile < 0 || !m_tiles[tile].url.isLocalFile()) {
        event->ignore();
        return;
    }
    if (KIO::DropJob *job = DragAndDropHelper::dropUrls(m_tiles[tile].url, event, this)) {
        connect(job, &KJob::result, this, [this](KJob *job) {
            if (job->error() && job->error() != KIO::ERR_USER_CANCELED) {
                Q_EMIT errorMessage(job->errorString());
            }
        });
    }
    event->acceptProposedAction();
}
