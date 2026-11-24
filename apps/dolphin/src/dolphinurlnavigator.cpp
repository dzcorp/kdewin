/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dolphinurlnavigator.h"

#include "dolphin_generalsettings.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinurlnavigatorscontroller.h"
#include "global.h"
#include "lumen/lumennames.h"
#include "panels/places/placespanel.h"
#include "lumentokens.h"
#include "lumenglyph.h"

#include <KIO/Global>
#include <KLocalizedString>
#include <KFilePlacesModel>
#include <KUrlComboBox>

#include <QAbstractButton>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QCursor>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QTimer>

#include <memory>
#include <QToolButton>

namespace
{
constexpr int CrumbPadding = 5; // KUrlNavigatorButton's

/// "This PC ›" ahead of a drive's folders, drawn and handled like KIO's path buttons; its arrow lists the drives.
class ThisPcCrumb : public QPushButton
{
public:
    explicit ThisPcCrumb(DolphinUrlNavigator *navigator)
        : QPushButton(ThisPc::title(), navigator)
        , m_navigator(navigator)
    {
        setFocusPolicy(Qt::TabFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setAttribute(Qt::WA_LayoutUsesWidgetRect);
        setMouseTracking(true);
        setProperty("_lumen_crumb", true);
        connect(this, &QPushButton::pressed, navigator, &KUrlNavigator::requestActivation);
        connect(this, &QPushButton::clicked, this, [this] {
            m_navigator->setLocationUrl(ThisPc::url());
        });
    }

    QSize sizeHint() const override
    {
        return {CrumbPadding + fontMetrics().horizontalAdvance(text()) + arrowWidth() + CrumbPadding, QPushButton::sizeHint().height()};
    }

    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        if (underMouse() || m_menuOpen) {
            QStyleOptionButton button;
            button.initFrom(this);
            button.rect = rect();
            button.features = QStyleOptionButton::Flat;
            style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &button, &p, this);
        }
        const QColor ink = palette().color(QPalette::Normal, QPalette::Text);
        const QRect label(CrumbPadding, 0, width() - arrowWidth() - CrumbPadding, height());
        p.setPen(ink);
        p.drawText(label, Qt::AlignVCenter, text());
        QStyleOption arrow;
        arrow.initFrom(this);
        arrow.palette.setColor(QPalette::Text, ink);
        arrow.palette.setColor(QPalette::WindowText, ink);
        arrow.palette.setColor(QPalette::ButtonText, ink);
        arrow.rect = QRect(label.right(), 0, arrowWidth(), height());
        if (!m_overArrow && !m_menuOpen) {
            arrow.state = QStyle::State_None;
        }
        style()->drawPrimitive(QStyle::PE_IndicatorArrowRight, &arrow, &p, this);
    }

    void enterEvent(QEnterEvent *event) override
    {
        QPushButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QPushButton::leaveEvent(event);
        m_overArrow = false;
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (const bool over = aboveArrow(event->position().x()); over != m_overArrow) {
            m_overArrow = over;
            update();
        }
        QPushButton::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::MiddleButton) {
            m_middlePressed = true;
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && aboveArrow(event->position().x())) {
            m_navigator->requestActivation();
            showDrives();
            return;
        }
        QPushButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::MiddleButton && m_middlePressed) {
            m_middlePressed = false;
            if (rect().contains(event->position().toPoint())) {
                Q_EMIT m_navigator->tabRequested(ThisPc::url());
            }
            return;
        }
        QPushButton::mouseReleaseEvent(event);
    }

private:
    int arrowWidth() const
    {
        return std::max(4, height() / 2);
    }

    bool aboveArrow(qreal x) const
    {
        return x >= width() - arrowWidth();
    }

    void showDrives()
    {
        KFilePlacesModel *places = DolphinPlacesModelSingleton::instance().placesModel();
        const QModelIndex current = places->closestItem(m_navigator->locationUrl());
        QMenu menu(this);
        for (int row = 0; row < places->rowCount(); ++row) {
            const QModelIndex place = places->index(row, 0);
            if (!PlacesPanel::isDriveGroup(places->groupType(place)) || places->isHidden(place)) {
                continue;
            }
            QAction *action = menu.addAction(place.data(Qt::DecorationRole).value<QIcon>(), places->text(place));
            if (place == current) {
                QFont bold = action->font();
                bold.setBold(true);
                action->setFont(bold);
            }
            connect(action, &QAction::triggered, this, [this, place = QPersistentModelIndex(place)] {
                open(place);
            });
        }
        m_menuOpen = true;
        update();
        menu.exec(mapToGlobal(QPoint(width() - arrowWidth() - CrumbPadding, height())));
        m_menuOpen = false;
        m_overArrow = rect().contains(mapFromGlobal(QCursor::pos())) && aboveArrow(mapFromGlobal(QCursor::pos()).x());
        update();
    }

    void open(const QPersistentModelIndex &place)
    {
        KFilePlacesModel *places = DolphinPlacesModelSingleton::instance().placesModel();
        if (!places->setupNeeded(place)) {
            m_navigator->setLocationUrl(places->url(place));
            return;
        }
        auto done = std::make_shared<QMetaObject::Connection>();
        *done = connect(places, &KFilePlacesModel::setupDone, this, [this, place, done](const QModelIndex &index, bool success) {
            if (index != place) {
                return;
            }
            disconnect(*done);
            if (success) {
                m_navigator->setLocationUrl(DolphinPlacesModelSingleton::instance().placesModel()->url(index));
            }
        });
        places->requestSetup(place);
    }

    DolphinUrlNavigator *m_navigator;
    bool m_overArrow = false;
    bool m_menuOpen = false;
    bool m_middlePressed = false;
};
}

DolphinUrlNavigator::DolphinUrlNavigator(QWidget *parent)
    : DolphinUrlNavigator(QUrl(), parent)
{
}

DolphinUrlNavigator::DolphinUrlNavigator(const QUrl &url, QWidget *parent)
    : KUrlNavigator(DolphinPlacesModelSingleton::instance().placesModel(), url, parent)
    , m_locationIcon(new QLabel(this))
    , m_recentButton(new QToolButton(this))
{
    QFont body = font();
    body.setPixelSize(int(Lumen::T::Type::Body[0]));
    setFont(body);
    // Windows address bar: the location's icon leads the path.
    m_locationIcon->setFixedWidth(30);
    m_locationIcon->setAlignment(Qt::AlignCenter);
    static_cast<QBoxLayout *>(layout())->insertWidget(0, m_locationIcon);
    connect(this, &KUrlNavigator::urlChanged, this, &DolphinUrlNavigator::updateLocationIcon);
    updateLocationIcon();
    const GeneralSettings *settings = GeneralSettings::self();
    setUrlEditable(settings->editableUrl());
    setShowFullPath(settings->showFullPath());
    setHomeUrl(Dolphin::homeUrl());
    setPlacesSelectorVisible(DolphinUrlNavigatorsController::placesSelectorVisible());

    // Windows address bar ends in a drop-down of recent locations; the editable combo box brings its own.
    m_recentButton->setAutoRaise(true);
    m_recentButton->setFocusPolicy(Qt::NoFocus);
    m_recentButton->setIcon(Lumen::glyphIcon(Lumen::Glyph::ChevronDown));
    m_recentButton->setToolTip(i18nc("@info:tooltip", "Recent locations"));
    auto *row = static_cast<QBoxLayout *>(layout());
    row->insertWidget(row->count() - 1, m_recentButton);
    connect(m_recentButton, &QToolButton::clicked, this, &DolphinUrlNavigator::showRecentLocations);

    // Edit mode is one text field: no scheme selector next to an empty field, no apply button; leaving the field restores
    // the path. KUrlNavigator shows and hides the scheme selector itself, so it is collapsed rather than hidden.
    for (QObject *child : children()) {
        if (child->inherits("KDEPrivate::KUrlNavigatorToggleButton")) {
            m_editToggle = static_cast<QWidget *>(child);
        } else if (child->inherits("KDEPrivate::KUrlNavigatorSchemeCombo")) {
            auto *schemes = static_cast<QWidget *>(child);
            schemes->setMaximumWidth(0);
            schemes->setFocusPolicy(Qt::NoFocus);
        }
    }
    editor()->lineEdit()->installEventFilter(this);
    connect(this, &KUrlNavigator::editableStateChanged, this, &DolphinUrlNavigator::updateEditMode);
    updateEditMode(isUrlEditable());
    editor()->setCompletionMode(KCompletion::CompletionMode(settings->urlCompletionMode()));
    setWhatsThis(xi18nc("@info:whatsthis location bar",
                        "<para>This describes the location of the files and folders "
                        "displayed below.</para><para>The name of the currently viewed "
                        "folder can be read at the very right. To the left of it is the "
                        "name of the folder that contains it. The whole line is called "
                        "the <emphasis>path</emphasis> to the current location because "
                        "following these folders from left to right leads here.</para>"
                        "<para>This interactive path "
                        "is more powerful than one would expect. To learn more "
                        "about the basic and advanced features of the location bar "
                        "<link url='help:/dolphin/location-bar.html'>click here</link>. "
                        "This will open the dedicated page in the Handbook.</para>"));

    DolphinUrlNavigatorsController::registerDolphinUrlNavigator(this);

    connect(this, &KUrlNavigator::returnPressed, this, &DolphinUrlNavigator::slotReturnPressed);

    auto readOnlyBadge = new QLabel();
    readOnlyBadge->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-readonly")).pixmap(12, 12));
    readOnlyBadge->setToolTip(i18nc("@info:tooltip of a 'locked' symbol in url navigator", "This folder is not writable for you."));
    readOnlyBadge->hide();
    setBadgeWidget(readOnlyBadge);
}

void DolphinUrlNavigator::setPlacesSelectorVisible(bool visible)
{
    KUrlNavigator::setPlacesSelectorVisible(visible);
    m_locationIcon->setVisible(!visible && !isUrlEditable());
}

void DolphinUrlNavigator::updateEditMode(bool editable)
{
    m_locationIcon->setVisible(!editable && !isPlacesSelectorVisible()); // the editor shows the location's icon itself
    m_recentButton->setHidden(editable);
    if (m_editToggle) {
        m_editToggle->setHidden(editable);
    }
}

bool DolphinUrlNavigator::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusOut && watched == editor()->lineEdit()) {
        const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason && !GeneralSettings::editableUrl()
            && locationUrl().scheme() != QLatin1String("remote")) {
            QTimer::singleShot(0, this, [this]() {
                if (isUrlEditable() && !editor()->lineEdit()->hasFocus()) {
                    setUrlEditable(false);
                }
            });
        }
    }
    return KUrlNavigator::eventFilter(watched, event);
}

// KIO centres the folder names by the font's line box; Segoe UI's deep descent leaves the capitals 1 px low in the
// field. Path buttons one pixel shorter, kept at the top, centre them (their separators follow the button).
bool DolphinUrlNavigator::event(QEvent *event)
{
    // The navigator has a fixed height; path buttons take it as their minimum.
    if (event->type() == QEvent::LayoutRequest && maximumHeight() < QWIDGETSIZE_MAX) {
        const int h = maximumHeight() - 1;
        for (QWidget *button : findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
            if (button->inherits("KDEPrivate::KUrlNavigatorButton") && button->maximumHeight() != h) {
                button->setFixedHeight(h);
                layout()->setAlignment(button, Qt::AlignTop);
            }
        }
    }
    if (event->type() == QEvent::LayoutRequest) {
        updateThisPcCrumb();
    }
    // KIO names the first button after a place or the scheme; This PC is no place.
    if (event->type() == QEvent::LayoutRequest && ThisPc::is(locationUrl())) {
        for (int i = 0; i < layout()->count(); ++i) {
            QWidget *widget = layout()->itemAt(i)->widget();
            if (widget && widget->inherits("KDEPrivate::KUrlNavigatorButton") && !widget->isHidden()) {
                auto *button = static_cast<QAbstractButton *>(widget);
                if (const QString title = ThisPc::title(); button->text() != title) {
                    button->setText(title);
                    button->setMinimumWidth(std::clamp(button->sizeHint().width(), 40, 150));
                }
                break;
            }
        }
    }
    return KUrlNavigator::event(event);
}

// File Explorer leads a drive's path with "This PC ›"; KIO starts it at the drive's place.
void DolphinUrlNavigator::updateThisPcCrumb()
{
    KFilePlacesModel *places = DolphinPlacesModelSingleton::instance().placesModel();
    const QModelIndex place = isUrlEditable() || showFullPath() ? QModelIndex() : places->closestItem(locationUrl());
    auto *row = static_cast<QBoxLayout *>(layout());
    QWidget *first = nullptr;
    for (int i = 0; place.isValid() && !first && i < row->count(); ++i) {
        QWidget *widget = row->itemAt(i)->widget();
        if (widget && widget->inherits("KDEPrivate::KUrlNavigatorButton")) {
            first = widget;
        }
    }
    if (!first || !PlacesPanel::isDriveGroup(places->groupType(place))) {
        if (m_thisPcCrumb) {
            m_thisPcCrumb->hide();
        }
        return;
    }
    if (!m_thisPcCrumb) {
        m_thisPcCrumb = new ThisPcCrumb(this);
    }
    if (row->indexOf(m_thisPcCrumb) != row->indexOf(first) - 1) {
        row->removeWidget(m_thisPcCrumb);
        row->insertWidget(row->indexOf(first), m_thisPcCrumb);
    }
    if (maximumHeight() < QWIDGETSIZE_MAX && m_thisPcCrumb->maximumHeight() != maximumHeight() - 1) {
        m_thisPcCrumb->setFixedHeight(maximumHeight() - 1);
        row->setAlignment(m_thisPcCrumb, Qt::AlignTop);
    }
    m_thisPcCrumb->setVisible(!first->isHidden());
}

void DolphinUrlNavigator::updateLocationIcon()
{
    const QIcon icon = QIcon::fromTheme(KIO::iconNameForUrl(locationUrl()), QIcon::fromTheme(QStringLiteral("folder")));
    m_locationIcon->setPixmap(icon.pixmap(QSize(16, 16), devicePixelRatioF()));
}

void DolphinUrlNavigator::showRecentLocations()
{
    QMenu menu(this);
    QList<QUrl> shown;
    for (int i = 0; i < historySize() && shown.size() < 12; ++i) {
        const QUrl url = locationUrl(i).adjusted(QUrl::StripTrailingSlash);
        if (shown.contains(url)) {
            continue;
        }
        shown.append(url);
        QAction *action = menu.addAction(QIcon::fromTheme(KIO::iconNameForUrl(url)), ThisPc::is(url) ? ThisPc::title() : url.toDisplayString(QUrl::PreferLocalFile));
        connect(action, &QAction::triggered, this, [this, url]() {
            setLocationUrl(url);
        });
    }
    menu.setMinimumWidth(width());
    menu.exec(mapToGlobal(QPoint(0, height())));
}

DolphinUrlNavigator::~DolphinUrlNavigator()
{
    DolphinUrlNavigatorsController::unregisterDolphinUrlNavigator(this);
}

QSize DolphinUrlNavigator::sizeHint() const
{
    if (isUrlEditable()) {
        return editor()->lineEdit()->sizeHint();
    }
    int widthHint = 0;
    for (int i = 0; i < layout()->count(); ++i) {
        QWidget *widget = layout()->itemAt(i)->widget();
        const QAbstractButton *button = qobject_cast<QAbstractButton *>(widget);
        if (button && button->icon().isNull()) {
            widthHint += widget->minimumSizeHint().width();
        }
    }
    if (readOnlyBadgeVisible()) {
        widthHint += badgeWidget()->sizeHint().width();
    }
    for (const QWidget *extra : {static_cast<QWidget *>(m_locationIcon), static_cast<QWidget *>(m_recentButton)}) {
        if (!extra->isHidden()) {
            widthHint += extra->sizeHint().width();
        }
    }
    return QSize(widthHint, KUrlNavigator::sizeHint().height());
}

std::unique_ptr<DolphinUrlNavigator::VisualState> DolphinUrlNavigator::visualState() const
{
    std::unique_ptr<VisualState> visualState{new VisualState};
    visualState->isUrlEditable = (isUrlEditable());
    const QLineEdit *lineEdit = editor()->lineEdit();
    visualState->hasFocus = lineEdit->hasFocus();
    visualState->text = lineEdit->text();
    visualState->cursorPosition = lineEdit->cursorPosition();
    visualState->selectionStart = lineEdit->selectionStart();
    visualState->selectionLength = lineEdit->selectionLength();
    return visualState;
}

void DolphinUrlNavigator::setVisualState(const VisualState &visualState)
{
    setUrlEditable(visualState.isUrlEditable);
    if (!visualState.isUrlEditable) {
        return;
    }
    editor()->lineEdit()->setText(visualState.text);
    if (visualState.hasFocus) {
        editor()->lineEdit()->setFocus();
        editor()->lineEdit()->setCursorPosition(visualState.cursorPosition);
        if (visualState.selectionStart != -1) {
            editor()->lineEdit()->setSelection(visualState.selectionStart, visualState.selectionLength);
        }
    }
}

void DolphinUrlNavigator::clearText() const
{

    editor()->lineEdit()->clear();
}

void DolphinUrlNavigator::setPlaceholderText(const QString &text)
{
    editor()->lineEdit()->setPlaceholderText(text);
}

void DolphinUrlNavigator::setReadOnlyBadgeVisible(bool visible)
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        readOnlyBadge->setVisible(visible);
    }
}

bool DolphinUrlNavigator::readOnlyBadgeVisible() const
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        return readOnlyBadge->isVisible();
    }
    return false;
}

void DolphinUrlNavigator::slotReturnPressed()
{
    if (!GeneralSettings::editableUrl()) {
        setUrlEditable(false);
    }
}

void DolphinUrlNavigator::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape && !isUrlEditable()) {
        Q_EMIT requestToLoseFocus();
        return;
    }
    KUrlNavigator::keyPressEvent(keyEvent);
}

#include "moc_dolphinurlnavigator.cpp"
