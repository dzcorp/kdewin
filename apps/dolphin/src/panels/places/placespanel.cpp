/*
 * SPDX-FileCopyrightText: 2008-2012 Peter Penz <peter.penz19@gmail.com>
 * SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>
 *
 * Based on KFilePlacesView from kdelibs:
 * SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
 * SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "placespanel.h"

#include "dolphin_generalsettings.h"
#include "dolphin_placespanelsettings.h"
#include "dolphinplacesmodelsingleton.h"
#include "lumencolor.h"
#include "lumenconfig.h"
#include "lumenglyph.h"
#include "settings/dolphinsettingsdialog.h"
#include "views/draganddrophelper.h"

#include <KFilePlacesModel>
#include <KIO/DropJob>
#include <KIO/Job>
#include <KLocalizedString>
#include <KProtocolManager>

#include <QAbstractItemDelegate>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>

#include <Solid/StorageAccess>

namespace
{
constexpr int Gutter = 4; // selection inset from the pane edges
constexpr int PillRoom = 8; // space before the section title for the selection pill
constexpr int GlyphSize = 12;
constexpr int BaseIconX = 4; // KFilePlacesViewDelegate's icon margin

QString sectionLabel(const QModelIndex &index)
{
    return index.data(KFilePlacesModel::GroupRole).toString();
}
}

// Wraps KFilePlacesViewDelegate, which stays the view's itemDelegate() because KIO casts it back; adds Windows navigation
// pane selection, pins and folded sections.
class PlacesSectionDelegate : public QAbstractItemDelegate
{
public:
    PlacesSectionDelegate(QAbstractItemDelegate *base, PlacesPanel *view)
        : QAbstractItemDelegate(view)
        , m_base(base)
        , m_view(view)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (m_view->isDriveRow(index)) {
            return QSize(option.rect.width(), 0);
        }
        if (m_view->isFolded(index)) {
            return QSize(option.rect.width(), m_view->isSectionStart(index) ? m_view->sectionHeaderHeight() : 0);
        }
        // File Explorer's navigation pane rows.
        return m_base->sizeHint(option, index).expandedTo(QSize(0, PlacesPanel::RowHeight));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (m_view->isFolded(index) || m_view->isDriveRow(index)) {
            return;
        }
        QRect row = option.rect;
        if (m_view->isSectionStart(index)) {
            row.setTop(row.top() + m_view->sectionHeaderHeight());
        }
        const auto &f = Lumen::fluent(option.palette);
        PlacesPanel::paintRowBackground(painter, option, row, (option.state & QStyle::State_MouseOver) && m_view->m_hoveredSection < 0);

        // KFilePlacesViewDelegate places text at absolute x, so the icon column is made by translating, not by moving the rect.
        constexpr int Shift = PlacesPanel::IconX - BaseIconX;
        QStyleOptionViewItem plain(option);
        plain.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
        plain.rect.setWidth(plain.rect.width() - Shift);
        painter->save();
        painter->translate(Shift, 0);
        m_base->paint(painter, plain, index);
        painter->restore();

        const auto *places = static_cast<const KFilePlacesModel *>(index.model());
        if (places->groupType(index) != KFilePlacesModel::PlacesType || sectionLabel(index).isEmpty()) {
            return;
        }
        const QRectF pin(row.right() - Gutter - 8 - GlyphSize, row.center().y() - GlyphSize / 2.0, GlyphSize, GlyphSize);
        const int textStart = PlacesPanel::IconX + m_view->iconSize().width() + 4;
        if (textStart + option.fontMetrics.horizontalAdvance(index.data(Qt::DisplayRole).toString()) + 8 > pin.left()) {
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillPath(Lumen::glyphPath(Lumen::Glyph::Pin, pin, m_view->devicePixelRatioF()), Lumen::argb(f.textSecondary));
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        return m_base->editorEvent(event, model, option, index);
    }

    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        return m_base->helpEvent(event, view, option, index);
    }

private:
    QAbstractItemDelegate *const m_base;
    PlacesPanel *const m_view;
};

PlacesPanel::PlacesPanel(QWidget *parent)
    : KFilePlacesView(parent)
{
    m_sectionDelegate = new PlacesSectionDelegate(itemDelegate(), this);
    viewport()->setMouseTracking(true);

    setDropOnPlaceEnabled(true);
    connect(this, &PlacesPanel::urlsDropped, this, &PlacesPanel::slotUrlsDropped);

    setAutoResizeItemsEnabled(false);

    setTeardownFunction([this](const QModelIndex &index) {
        slotTearDownRequested(index);
    });

    m_openInSplitView = std::make_unique<QAction>(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18nc("@action:inmenu", "Open in Split View"));
    m_openInSplitView->setPriority(QAction::HighPriority);
    connect(m_openInSplitView.get(), &QAction::triggered, this, [this]() {
        const QUrl url = currentIndex().data(KFilePlacesModel::UrlRole).toUrl();
        Q_EMIT openInSplitViewRequested(url);
    });
    addAction(m_openInSplitView.get());

    m_configureTrashAction = std::make_unique<QAction>(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Trash…"));
    m_configureTrashAction->setPriority(QAction::HighPriority);
    connect(m_configureTrashAction.get(), &QAction::triggered, this, &PlacesPanel::slotConfigureTrash);
    addAction(m_configureTrashAction.get());

    connect(this, &PlacesPanel::contextMenuAboutToShow, this, &PlacesPanel::slotContextMenuAboutToShow);

    connect(this, &PlacesPanel::iconSizeChanged, this, [](const QSize &newSize) {
        int iconSize = qMin(newSize.width(), newSize.height());
        if (iconSize == 0) {
            // Don't store 0 size, let's keep -1 for default/small/automatic
            iconSize = -1;
        }
        PlacesPanelSettings *settings = PlacesPanelSettings::self();
        settings->setIconSize(iconSize);
        settings->save();
    });

    readSettings();

    // Set the model here so that it's loaded in time for the sizeHint to properly apply (setting it upon showEvent is too late)
    auto *placesModel = DolphinPlacesModelSingleton::instance().placesModel();
    setModel(placesModel);

    connect(placesModel, &KFilePlacesModel::errorMessage, this, &PlacesPanel::errorMessage);
    connect(placesModel, &KFilePlacesModel::teardownDone, this, &PlacesPanel::slotTearDownDone);

    connect(placesModel, &QAbstractItemModel::rowsInserted, this, &PlacesPanel::slotRowsInserted);
    connect(placesModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &PlacesPanel::slotRowsAboutToBeRemoved);

    for (int i = 0; i < model()->rowCount(); ++i) {
        connectDeviceSignals(model()->index(i, 0, QModelIndex()));
    }
}

PlacesPanel::~PlacesPanel() = default;

void PlacesPanel::setUrl(const QUrl &url)
{
    KFilePlacesView::setUrl(url);
}

QList<QAction *> PlacesPanel::customContextMenuActions() const
{
    return m_customContextMenuActions;
}

void PlacesPanel::setCustomContextMenuActions(const QList<QAction *> &actions)
{
    m_customContextMenuActions = actions;
}

void PlacesPanel::proceedWithTearDown()
{
    if (m_indexToTearDown.isValid()) {
        auto *placesModel = static_cast<KFilePlacesModel *>(model());
        placesModel->requestTeardown(m_indexToTearDown);
    } else {
        qWarning() << "Places entry to tear down is no longer valid";
    }
}

void PlacesPanel::readSettings()
{
    if (GeneralSettings::autoExpandFolders()) {
        setDragAutoActivationDelay(750);
    } else {
        setDragAutoActivationDelay(0);
    }

    const int iconSize = qMax(0, PlacesPanelSettings::iconSize());
    setIconSize(QSize(iconSize, iconSize));
    m_foldedGroups = PlacesPanelSettings::collapsedSections();
    scheduleDelayedItemsLayout();
}

static bool isInternalDrag(const QMimeData *mimeData)
{
    const auto formats = mimeData->formats();
    for (const auto &format : formats) {
        // from KFilePlacesModel::_k_internalMimetype
        if (format.startsWith(QLatin1String("application/x-kfileplacesmodel-"))) {
            return true;
        }
    }
    return false;
}

void PlacesPanel::dragMoveEvent(QDragMoveEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid()) {
        auto *placesModel = static_cast<KFilePlacesModel *>(model());

        // Reject drag ontop of a non-writable protocol
        // We don't know whether we're dropping inbetween or ontop of a place
        // so still allow internal drag events so that re-arranging still works.
        if (!isInternalDrag(event->mimeData())) {
            const QUrl url = placesModel->url(index);
            if (!url.isValid() || !KProtocolManager::supportsWriting(url)) {
                event->setDropAction(Qt::IgnoreAction);
            } else {
                DragAndDropHelper::updateDropAction(event, url);
            }
        }
    }

    KFilePlacesView::dragMoveEvent(event);
}

void PlacesPanel::slotConfigureTrash()
{
    const QUrl url = currentIndex().data(KFilePlacesModel::UrlRole).toUrl();

    DolphinSettingsDialog *settingsDialog = new DolphinSettingsDialog(url, this);
    settingsDialog->setCurrentPage(settingsDialog->trashSettings);
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    settingsDialog->show();
}

void PlacesPanel::slotUrlsDropped(const QUrl &dest, QDropEvent *event, QWidget *parent)
{
    KIO::DropJob *job = DragAndDropHelper::dropUrls(dest, event, parent);
    if (job) {
        connect(job, &KIO::DropJob::result, this, [this](KJob *job) {
            if (job->error() && job->error() != KIO::ERR_USER_CANCELED) {
                Q_EMIT errorMessage(job->errorString());
            }
        });
    }
}

void PlacesPanel::slotContextMenuAboutToShow(const QModelIndex &index, QMenu *menu)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());
    // The view names sections by DolphinPlacesModel's invisible labels; put the real name in the menu entry.
    const QString label = sectionLabel(index);
    const QString shown = label.isEmpty() ? i18ndc("kio6", "@item:inmenu", "Hide Section") : i18ndc("kio6", "@item:inmenu", "Hide Section '%1'", label);
    const QString section = placesModel->KFilePlacesModel::data(index, KFilePlacesModel::GroupRole).toString();
    const auto menuActions = menu->actions();
    for (QAction *action : menuActions) {
        if (action->isCheckable() && action->text() == shown) {
            action->setText(i18ndc("kio6", "@item:inmenu", "Hide Section '%1'", section));
        }
    }
    const QUrl url = placesModel->url(index);
    const Solid::Device device = placesModel->deviceForIndex(index);

    m_configureTrashAction->setVisible(url.scheme() == QLatin1String("trash"));
    m_openInSplitView->setVisible(url.isValid());

    // show customContextMenuActions only on the view's context menu
    if (!url.isValid() && !device.isValid()) {
        addActions(m_customContextMenuActions);
    } else {
        const auto actions = this->actions();
        for (QAction *action : actions) {
            if (m_customContextMenuActions.contains(action)) {
                removeAction(action);
            }
        }
    }
}

void PlacesPanel::slotTearDownRequested(const QModelIndex &index)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
    if (!storageAccess) {
        return;
    }

    m_indexToTearDown = QPersistentModelIndex(index);

    // disconnect the Solid::StorageAccess::teardownRequested
    // to prevent emitting PlacesPanel::storageTearDownExternallyRequested
    // after we have emitted PlacesPanel::storageTearDownRequested
    disconnect(storageAccess, &Solid::StorageAccess::teardownRequested, this, &PlacesPanel::slotTearDownRequestedExternally);
    Q_EMIT storageTearDownRequested(storageAccess->filePath());
}

void PlacesPanel::slotTearDownRequestedExternally(const QString &udi)
{
    Q_UNUSED(udi);
    auto *storageAccess = static_cast<Solid::StorageAccess *>(sender());

    Q_EMIT storageTearDownExternallyRequested(storageAccess->filePath());
}

void PlacesPanel::slotTearDownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData)
{
    Q_UNUSED(errorData); // All error handling is currently done in frameworks.

    if (index == m_indexToTearDown) {
        if (error == Solid::ErrorType::NoError) {
            // No error; it must have been unmounted successfully
            Q_EMIT storageTearDownSuccessful();
        }
        m_indexToTearDown = QPersistentModelIndex();
    }
}

void PlacesPanel::slotRowsInserted(const QModelIndex &parent, int first, int last)
{
    for (int i = first; i <= last; ++i) {
        connectDeviceSignals(model()->index(i, 0, parent));
    }
}

void PlacesPanel::slotRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    for (int i = first; i <= last; ++i) {
        const QModelIndex index = placesModel->index(i, 0, parent);

        Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
        if (!storageAccess) {
            continue;
        }

        disconnect(storageAccess, &Solid::StorageAccess::teardownRequested, this, nullptr);
    }
}

void PlacesPanel::connectDeviceSignals(const QModelIndex &index)
{
    auto *placesModel = static_cast<KFilePlacesModel *>(model());

    Solid::StorageAccess *storageAccess = placesModel->deviceForIndex(index).as<Solid::StorageAccess>();
    if (!storageAccess) {
        return;
    }

    connect(storageAccess, &Solid::StorageAccess::teardownRequested, this, &PlacesPanel::slotTearDownRequestedExternally);
}

#include "moc_placespanel.cpp"

QAbstractItemDelegate *PlacesPanel::itemDelegateForIndex(const QModelIndex &) const
{
    return m_sectionDelegate;
}

void PlacesPanel::setDrivesInTree(bool inTree)
{
    m_drivesInTree = inTree;
    scheduleDelayedItemsLayout();
}

bool PlacesPanel::isDriveGroup(int groupType)
{
    return groupType == KFilePlacesModel::DevicesType || groupType == KFilePlacesModel::RemovableDevicesType;
}

bool PlacesPanel::isDriveRow(const QModelIndex &index) const
{
    return m_drivesInTree && index.isValid() && isDriveGroup(static_cast<const KFilePlacesModel *>(model())->groupType(index));
}

bool PlacesPanel::isSectionStart(const QModelIndex &index) const
{
    if (!index.isValid() || isRowHidden(index.row()) || isDriveRow(index)) {
        return false;
    }
    int previous = index.row() - 1;
    while (previous >= 0 && isRowHidden(previous)) {
        --previous;
    }
    return previous < 0 ? !sectionLabel(index).isEmpty() : sectionLabel(index) != sectionLabel(model()->index(previous, 0));
}

bool PlacesPanel::isFolded(const QModelIndex &index) const
{
    return !m_foldedGroups.isEmpty() && !sectionLabel(index).isEmpty()
        && m_foldedGroups.contains(static_cast<const KFilePlacesModel *>(model())->groupType(index));
}

// KFilePlacesViewDelegate::sectionHeaderHeight(): the room each section's first row reserves above itself.
int PlacesPanel::sectionHeaderHeight() const
{
    return fontMetrics().height() + 3 * (4 + spacing());
}

int PlacesPanel::sectionAt(const QPoint &pos) const
{
    const QModelIndex index = indexAt(pos);
    if (!isSectionStart(index) || pos.y() >= visualRect(index).top() + sectionHeaderHeight()) {
        return -1;
    }
    return index.row();
}

void PlacesPanel::toggleSection(int row)
{
    toggleGroup(static_cast<KFilePlacesModel *>(model())->groupType(model()->index(row, 0)));
}

bool PlacesPanel::isGroupFolded(int groupType) const
{
    return m_foldedGroups.contains(groupType);
}

void PlacesPanel::toggleGroup(int groupType)
{
    if (!m_foldedGroups.removeOne(groupType)) {
        m_foldedGroups.append(groupType);
    }
    PlacesPanelSettings::setCollapsedSections(m_foldedGroups);
    PlacesPanelSettings::self()->save();
    scheduleDelayedItemsLayout();
    viewport()->update();
    Q_EMIT foldedGroupsChanged();
}

void PlacesPanel::updateGeometries()
{
    KFilePlacesView::updateGeometries();
    if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff) {
        setFixedHeight(contentsSize().height() + 2 * frameWidth());
    }
}

void PlacesPanel::requestTearDownOf(const QModelIndex &index)
{
    slotTearDownRequested(index);
}

void PlacesPanel::paintRowBackground(QPainter *p, const QStyleOptionViewItem &option, const QRect &row, bool hovered)
{
    const bool dark = Lumen::isDark(option.palette);
    const auto &f = Lumen::fluent(dark);
    const qreal radius = Lumen::Config::get().controlRadius;
    // Hover and selection share one pixel-aligned plate; the selection's stroke lies inside it.
    const QRectF card = QRectF(row).adjusted(Gutter, 2, -Gutter, -2);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (option.state & QStyle::State_Selected) {
        p->setPen(QPen(Lumen::argb(f.controlStroke), 1));
        p->setBrush(Lumen::argb(hovered ? f.controlFillHover : f.controlFill));
        p->drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);
        const QRectF pill(card.left() + 0.5, card.center().y() - Lumen::T::Size::ListPillH / 2, Lumen::T::Size::ListPillW, Lumen::T::Size::ListPillH);
        p->setPen(Qt::NoPen);
        p->setBrush(Lumen::accentFill(option.palette.color(QPalette::Highlight), dark));
        p->drawRoundedRect(pill, Lumen::T::Radius::ListPill, Lumen::T::Radius::ListPill);
    } else if (hovered) {
        p->setPen(Qt::NoPen);
        p->setBrush(Lumen::argb(f.subtleFillHover));
        p->drawRoundedRect(card, radius, radius);
    }
    p->restore();
}

void PlacesPanel::paintSectionHeader(QPainter *p, const QWidget *widget, const QRect &row, const QString &title, bool folded, bool hovered)
{
    const auto &f = Lumen::fluent(widget->palette());
    const QColor ink = Lumen::argb(f.textSecondary);
    QFont font = widget->font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    const QFontMetrics metrics(font);
    const QRect area = row.adjusted(Gutter, 2, -Gutter, -2);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setFont(font);
    if (hovered) {
        const qreal radius = Lumen::Config::get().controlRadius;
        p->setPen(Qt::NoPen);
        p->setBrush(Lumen::argb(f.subtleFillHover));
        p->drawRoundedRect(QRectF(area), radius, radius);
    }
    const QRect text = area.adjusted(PillRoom + 4, 0, -(8 + GlyphSize + 8), 0);
    p->setPen(ink);
    p->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(title, Qt::ElideRight, text.width()));
    const QRectF chevron(area.right() + 1 - 8 - GlyphSize, area.center().y() + 1 - GlyphSize / 2.0, GlyphSize, GlyphSize);
    p->fillPath(Lumen::glyphPath(folded ? Lumen::Glyph::ChevronDown : Lumen::Glyph::ChevronUp, chevron, widget->devicePixelRatioF()), ink);
    p->restore();
}

void PlacesPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (const int row = sectionAt(event->position().toPoint()); row >= 0) {
            toggleSection(row);
            event->accept();
            return;
        }
    }
    KFilePlacesView::mousePressEvent(event);
}

void PlacesPanel::mouseMoveEvent(QMouseEvent *event)
{
    const int row = event->buttons() == Qt::NoButton ? sectionAt(event->position().toPoint()) : -1;
    if (row != m_hoveredSection) {
        m_hoveredSection = row;
        viewport()->update();
    }
    KFilePlacesView::mouseMoveEvent(event);
}

void PlacesPanel::leaveEvent(QEvent *event)
{
    if (m_hoveredSection >= 0) {
        m_hoveredSection = -1;
        viewport()->update();
    }
    KFilePlacesView::leaveEvent(event);
}

QModelIndex PlacesPanel::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    const bool up = cursorAction == MoveUp || cursorAction == MovePrevious;
    if (!up && cursorAction != MoveDown && cursorAction != MoveNext) {
        return KFilePlacesView::moveCursor(cursorAction, modifiers);
    }
    const int step = up ? -1 : 1;
    for (int row = currentIndex().row() + step; row >= 0 && row < model()->rowCount(); row += step) {
        const QModelIndex index = model()->index(row, 0);
        if (!isRowHidden(row) && !isFolded(index) && !isDriveRow(index)) {
            return index;
        }
    }
    return currentIndex();
}

void PlacesPanel::paintEvent(QPaintEvent *event)
{
    KFilePlacesView::paintEvent(event);
    const auto *places = static_cast<KFilePlacesModel *>(model());
    QPainter p(viewport());
    for (int row = 0; row < places->rowCount(); ++row) {
        const QModelIndex index = places->index(row, 0);
        if (!isSectionStart(index)) {
            continue;
        }
        const QRect r = visualRect(index);
        const QRect area(r.left(), r.top(), r.width(), sectionHeaderHeight());
        if (event->rect().intersects(area)) {
            const QString title = places->KFilePlacesModel::data(index, KFilePlacesModel::GroupRole).toString();
            paintSectionHeader(&p, this, area, title, isFolded(index), row == m_hoveredSection);
        }
    }
}
