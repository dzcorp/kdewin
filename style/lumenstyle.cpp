#include "lumenstyle.h"
#include "lumenanimations.h"
#include "lumenconfig.h"
#include "lumenshadowhelper.h"
#include "lumenwindowdrag.h"
#include "lumenwindowmaterial.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOption>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QWindow>
#include <QToolButton>
#include <QTreeView>

#include <KConfigWatcher>
#include <KSharedConfig>

#include <algorithm>
#include <cmath>

namespace Lumen
{
namespace
{
constexpr int ArrowArea = 32;
constexpr int LineEditTextMargin = 10;
constexpr int SliderTickSpace = 5; // QSlider::sizeHint() adds this per tick side
constexpr int SpinButton = 24;
constexpr int SpinArea = 2 * SpinButton + 4;
// Extra leading padding of the first item-view cell; leaves room for the selection pill.
constexpr int ItemLead = 6;
constexpr int GroupTitleGap = 6;
constexpr int GroupPad = 4;

bool leadingCell(const QStyleOptionViewItem *vi)
{
    const auto pos = vi->viewItemPosition;
    return vi->decorationPosition != QStyleOptionViewItem::Top
        && (pos == QStyleOptionViewItem::Beginning || pos == QStyleOptionViewItem::OnlyOne || pos == QStyleOptionViewItem::Invalid);
}

int px(double v)
{
    return int(std::lround(v));
}

bool isSettingsApp()
{
    static const bool settings = [] {
        const QString app = QCoreApplication::applicationName();
        return app == u"systemsettings" || app == u"kcmshell6" || app == u"kinfocenter";
    }();
    return settings;
}

// System Settings' page title (a KTitleWidget atop its module view) restyles its label through a style sheet on every
// text, font or palette change, so the Windows title rule is appended after each one.
constexpr const char *PageTitleRule = "/*lumen-title*/";

bool isPageTitle(const QWidget *w)
{
    return isSettingsApp() && w->inherits("KTitleWidget") && w->parentWidget() && w->parentWidget()->inherits("ModuleView");
}

void stylePageTitle(QLabel *label)
{
    const QString sheet = label->styleSheet();
    if (!sheet.contains(u"font-size") || sheet.contains(QLatin1String(PageTitleRule))) {
        return;
    }
    label->setStyleSheet(sheet
                         + QStringLiteral(" QLabel { font-family: \"%1\"; font-size: %2px; font-weight: %3 } %4")
                               .arg(QLatin1String(T::Font::UiDisplay))
                               .arg(px(T::Type::Title[0]))
                               .arg(px(T::Type::Title[2]))
                               .arg(QLatin1String(PageTitleRule)));
}

bool isPopupContainer(const QWidget *w)
{
    return w && w->inherits("QComboBoxPrivateContainer");
}

bool isTooltip(const QWidget *w)
{
    return w && (w->inherits("QTipLabel") || w->windowType() == Qt::ToolTip);
}
}

Style::Style()
    : m_anim(new Animations(this))
    , m_shadows(new ShadowHelper(this))
    , m_drag(new WindowDrag(this))
    , m_material(new WindowMaterial(this))
{
    setObjectName(QStringLiteral("Lumen"));
    // Live tuning from the Lumen settings module: re-read lumenrc and restyle open windows.
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig));
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this] {
        if (!Config::reload()) {
            return;
        }
        if (!Config::get().animations) {
            m_anim->stop();
        }
        const auto widgets = QApplication::allWidgets();
        for (QWidget *w : widgets) {
            QEvent ev(QEvent::StyleChange);
            QCoreApplication::sendEvent(w, &ev);
            w->update();
        }
    });
}

Style::~Style() = default;

qreal Style::anim(const QStyleOption *opt, const QWidget *w, quintptr key, bool on, int ms) const
{
    const QObject *target = w ? static_cast<const QObject *>(w) : opt->styleObject;
    return m_anim->value(target, key, on, ms);
}

void Style::polish(QWidget *w)
{
    QCommonStyle::polish(w);
    // Win32 text is Segoe UI; QML keeps the system font (Segoe UI Variable, as in XAML). Windows' own context menus are XAML.
    static const bool win32 = QFontDatabase::hasFamily(QLatin1String(T::Font::Win32));
    if (win32 && w->isWindow() && !w->testAttribute(Qt::WA_SetFont) && !qobject_cast<QMenu *>(w)) {
        QFont family;
        family.setFamily(QLatin1String(T::Font::Win32));
        w->setFont(family);
    }
    if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w) || qobject_cast<QLineEdit *>(w)
        || qobject_cast<QScrollBar *>(w) || qobject_cast<QSlider *>(w) || qobject_cast<QTabBar *>(w) || qobject_cast<QHeaderView *>(w)
        || qobject_cast<QMenuBar *>(w)) {
        w->setAttribute(Qt::WA_Hover);
    }
    if (auto *sb = qobject_cast<QScrollBar *>(w)) {
        sb->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
    if (auto *tabs = qobject_cast<QTabBar *>(w); tabs && isPageTabs(nullptr, w)) {
        tabs->setExpanding(false); // tab buttons hug their text
    }
    // QScrollArea fills its viewport and content with the window color (QScrollArea::setWidget), which hides Mica and
    // turns tab pages into grey panels; window-colored scroll areas stay see-through.
    if (auto *area = qobject_cast<QScrollArea *>(w)) {
        if (area->viewport()->backgroundRole() == QPalette::Window || area->viewport()->backgroundRole() == QPalette::NoRole) {
            area->viewport()->setAutoFillBackground(false);
            if (QWidget *content = area->widget(); content && content->backgroundRole() == QPalette::Window) {
                content->setAutoFillBackground(false);
            }
        }
    } else if (w->parentWidget() && w->backgroundRole() == QPalette::Window) {
        auto *area = qobject_cast<QScrollArea *>(w->parentWidget()->parentWidget());
        if (area && area->widget() == w && !area->viewport()->autoFillBackground()) {
            w->setAutoFillBackground(false);
        }
    }
    if (auto *view = qobject_cast<QAbstractItemView *>(w)) {
        view->viewport()->setAttribute(Qt::WA_Hover);
    }
    const bool menu = qobject_cast<QMenu *>(w) != nullptr;
    const bool popup = isPopupContainer(w);
    if (menu || popup || isTooltip(w)) {
        // Keep alpha available so an existing popup can switch materials live.
        if (QWindow *win = w->windowHandle(); win && w->testAttribute(Qt::WA_WState_Created) && !win->isVisible() && !win->format().hasAlpha()) {
            win->destroy();
        }
        w->setAttribute(Qt::WA_TranslucentBackground);
        w->setAutoFillBackground(false);
        m_shadows->registerWidget(w, isTooltip(w) ? ShadowHelper::Kind::Tooltip : ShadowHelper::Kind::Flyout);
    }
    if (popup) {
        if (auto *view = w->findChild<QAbstractItemView *>()) {
            view->setFrameShape(QFrame::NoFrame);
            view->viewport()->setAutoFillBackground(false);
            view->setAutoFillBackground(false);
            QPalette pal = view->palette();
            pal.setColor(QPalette::Base, Qt::transparent);
            view->setPalette(pal);
        }
    }
    if (isPageTitle(w)) {
        // Same height as the QML page headers beside the sidebar.
        w->setContentsMargins(w->contentsMargins().left(), 4, w->contentsMargins().right(), 4);
        for (auto *label : w->findChildren<QLabel *>(Qt::FindDirectChildrenOnly)) {
            label->installEventFilter(this);
            stylePageTitle(label);
        }
    }
    m_drag->registerWidget(w);
    m_material->registerWidget(w);
}

bool Style::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::StyleChange) {
        if (auto *label = qobject_cast<QLabel *>(o)) {
            stylePageTitle(label);
        }
    }
    return QCommonStyle::eventFilter(o, e);
}

bool Style::isSettingsCard(const QWidget *w)
{
    if (!qobject_cast<const QGroupBox *>(w) || !isSettingsApp()) {
        return false;
    }
    for (const QWidget *p = w->parentWidget(); p; p = p->parentWidget()) {
        if (qobject_cast<const QGroupBox *>(p)) {
            return false;
        }
    }
    return true;
}

void Style::unpolish(QWidget *w)
{
    m_shadows->unregisterWidget(w);
    m_drag->unregisterWidget(w);
    m_material->unregisterWidget(w);
    QCommonStyle::unpolish(w);
}

int Style::pixelMetric(PixelMetric m, const QStyleOption *opt, const QWidget *w) const
{
    switch (m) {
    case PM_DefaultFrameWidth:
    case PM_ComboBoxFrameWidth:
    case PM_SpinBoxFrameWidth:
        return px(T::Size::Border);
    case PM_ButtonMargin:
        return px(T::Size::ControlPadX);
    case PM_ButtonDefaultIndicator:
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
    case PM_MenuBarPanelWidth:
    case PM_ToolBarFrameWidth:
    case PM_ScrollView_ScrollBarSpacing:
    case PM_ScrollView_ScrollBarOverlap:
    case PM_TabBarTabOverlap:
    case PM_TabBarBaseOverlap:
    case PM_TabBarTabShiftHorizontal:
    case PM_TabBarTabShiftVertical:
    case PM_MenuTearoffHeight:
    case PM_DockWidgetFrameWidth:
        return 0;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return px(T::Size::Checkbox);
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return px(T::Size::Radio);
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 8;
    case PM_ScrollBarExtent:
        return px(T::Size::Scrollbar);
    case PM_ScrollBarSliderMin:
        return px(T::Size::ScrollbarMinThumb);
    case PM_SliderThickness:
        return px(T::Size::ControlH); // WinUI Slider MinHeight; leaves room for the thumb's focus ring
    case PM_SliderLength:
    case PM_SliderControlThickness:
        return px(T::Size::SliderThumb);
    case PM_SliderTickmarkOffset:
        return SliderTickSpace;
    case PM_MenuHMargin:
        return px(T::Size::MenuPad);
    case PM_MenuVMargin:
        return px(T::Size::MenuPadV);
    case PM_MenuPanelWidth:
        return 1;
    case PM_SubMenuOverlap:
        return -px(T::Size::MenuPad) - 1;
    case PM_MenuBarItemSpacing:
        return 0;
    case PM_MenuBarHMargin:
    case PM_MenuBarVMargin:
        return 2;
    case PM_SmallIconSize:
    case PM_ButtonIconSize:
        return 16;
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
        return 8;
    case PM_LayoutLeftMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutBottomMargin:
        if (qobject_cast<const QGroupBox *>(w)) {
            return 0;
        }
        return (w && w->isWindow()) ? 12 : 8;
    case PM_TabBarTabHSpace:
        // Title-row tabs keep their concave feet clear of icon, text and close button.
        return isTitleTabs(w) ? 24 + 2 * TitleTabFlare : 24;
    case PM_TabBarTabVSpace:
        return 12;
    case PM_TabBar_ScrollButtonOverlap:
        return 1;
    case PM_ToolTipLabelFrameWidth:
        return 8;
    case PM_HeaderMargin:
        return 6;
    case PM_ToolBarItemSpacing:
    case PM_ToolBarItemMargin:
        // Client-drawn title rows reach the window edges.
        if (w && w->property("_lumen_titlerow").toBool()) {
            return 0;
        }
        // Explorer: address row 48, command bar 46 (32 px controls).
        if (w && m == PM_ToolBarItemMargin && w->property("_lumen_navrow").toBool()) {
            return 8;
        }
        return w && m == PM_ToolBarItemMargin && w->property("_lumen_commandrow").toBool() ? 8 : 2;
    case PM_ToolBarHandleExtent:
        return 6;
    case PM_ToolBarSeparatorExtent:
        return 9;
    case PM_SplitterWidth:
        return 1;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    case PM_MenuButtonIndicator:
        return 16;
    case PM_DockWidgetTitleMargin:
        return 6;
    case PM_DockWidgetSeparatorExtent:
        return 1;
    default:
        break;
    }
    return QCommonStyle::pixelMetric(m, opt, w);
}

int Style::styleHint(StyleHint h, const QStyleOption *opt, const QWidget *w, QStyleHintReturn *ret) const
{
    if (h == SH_KCustomElement) {
        return w && w->objectName() == QLatin1String("CE_CapacityBar") ? int(CE_CapacityBar) : 0;
    }
    switch (h) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
    case SH_DockWidget_ButtonsHaveFrame:
    case SH_UnderlineShortcut:
    case SH_ScrollBar_Transient:
    case SH_Menu_FlashTriggeredItem:
    case SH_ComboBox_ListMouseTracking:
    case SH_MessageBox_CenterButtons:
        return false;
    case SH_ToolTip_Mask:
    case SH_Menu_Mask:
        return false;
    case SH_ItemView_ShowDecorationSelected:
    case SH_ComboBox_Popup:
    case SH_Menu_Scrollable:
    case SH_Menu_SupportsSections:
    case SH_ScrollView_FrameOnlyAroundContents:
    case SH_TitleBar_NoBorder:
    case SH_Menu_MouseTracking:
    case SH_MenuBar_MouseTracking:
    case SH_ItemView_ArrowKeysNavigateIntoChildren:
    case SH_Slider_StopMouseOverSlider:
        return true;
    case SH_Widget_Animation_Duration:
        return Config::get().duration(int(T::Motion::FastMs));
    case SH_Slider_AbsoluteSetButtons:
        return Qt::LeftButton;
    case SH_ComboBox_PopupFrameStyle:
        return QFrame::NoFrame;
    case SH_Table_GridLineColor:
        return opt ? int(argb(fluent(opt->palette).divider).rgba()) : 0;
    case SH_FocusFrame_AboveWidget:
        return true;
    case SH_Menu_SubMenuPopupDelay:
        return 200;
    case SH_ToolBox_SelectedPageTitleBold:
        return false;
    default:
        return QCommonStyle::styleHint(h, opt, w, ret);
    }
}

QSize Style::sizeFromContents(ContentsType ct, const QStyleOption *opt, const QSize &sz, const QWidget *w) const
{
    const int ch = px(T::Size::ControlH);
    switch (ct) {
    case CT_PushButton: {
        const auto *b = qstyleoption_cast<const QStyleOptionButton *>(opt);
        int width = sz.width() + 2 * px(T::Size::ControlPadX);
        if (hidesButtonIcon(b)) {
            width -= b->iconSize.width() + 4; // QPushButton::sizeHint() reserved the icon and its gap
        }
        if (b && (b->features & QStyleOptionButton::HasMenu)) {
            width += 16;
        }
        if (b && !b->text.isEmpty()) {
            width = std::max(width, 80);
        }
        return {width, std::max(sz.height() + 2 * px(T::Size::ControlPadY), ch)};
    }
    case CT_ToolButton: {
        const auto *tb = qstyleoption_cast<const QStyleOptionToolButton *>(opt);
        int width = sz.width() + 12;
        if (tb && (tb->features & QStyleOptionToolButton::MenuButtonPopup)) {
            width += pixelMetric(PM_MenuButtonIndicator, opt, w);
        } else if (tb && (tb->features & QStyleOptionToolButton::HasMenu) && tb->toolButtonStyle != Qt::ToolButtonIconOnly) {
            width += 12;
        }
        return {width, std::max(sz.height() + 10, tb && tb->toolButtonStyle == Qt::ToolButtonTextUnderIcon ? 0 : ch)};
    }
    case CT_LineEdit:
        return {sz.width() + 2 * LineEditTextMargin, std::max(sz.height() + 8, ch)};
    case CT_ComboBox: {
        // Padding on both sides of the text (the right one separates it from the arrow).
        return {sz.width() + 2 * px(T::Size::ControlPadX) + ArrowArea, std::max(sz.height() + 2 * px(T::Size::ControlPadY), ch)};
    }
    case CT_ProgressBar: {
        // Qt Quick progress bars take their thickness from here; the bar itself is T::Size::ProgressH.
        const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
        const int thick = px(T::Size::ProgressH) + 2;
        return (!pb || (pb->state & State_Horizontal)) ? QSize(sz.width(), std::max(sz.height(), thick)) : QSize(std::max(sz.width(), thick), sz.height());
    }
    case CT_SpinBox:
        // `sz` already holds the embedded line edit's padded height.
        return {sz.width() + 24 + SpinArea, std::max(opt->fontMetrics.height() + 2 * px(T::Size::ControlPadY), ch)};
    case CT_GroupBox: {
        // QGroupBox measures its title with the regular font; the title is drawn semibold.
        const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);
        const int title = gb ? QFontMetrics(groupTitleFont(w)).horizontalAdvance(gb->text) - opt->fontMetrics.horizontalAdvance(gb->text) : 0;
        return {sz.width() + std::max(0, title) + 4, sz.height() + 2 * GroupPad + GroupTitleGap};
    }
    case CT_Slider: {
        // Qt Quick sliders size themselves from here; keep room for the thumb and its focus ring.
        const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(opt);
        const bool horizontal = !sl || sl->orientation == Qt::Horizontal;
        return horizontal ? QSize(sz.width(), std::max(sz.height(), ch)) : QSize(std::max(sz.width(), ch), sz.height());
    }
    case CT_CheckBox:
    case CT_RadioButton: {
        const QSize s = QCommonStyle::sizeFromContents(ct, opt, sz, w);
        return {s.width(), std::max(s.height(), 24)};
    }
    case CT_MenuItem: {
        const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
        if (!mi) {
            break;
        }
        if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
            return {sz.width(), 2 * px(T::Size::MenuSeparatorPad) + 1};
        }
        // Combo popups pass the item's current rect, not its contents: measuring it grows rows on every relayout.
        const QSize content = qobject_cast<const QComboBox *>(w) ? QSize(mi->fontMetrics.horizontalAdvance(mi->text), mi->fontMetrics.height()) : sz;
        int width = content.width() + 2 * px(T::Size::MenuItemPadX) + px(T::Size::MenuIconSlot) + 2 * px(T::Size::MenuPad);
        if (mi->text.contains(QLatin1Char('\t'))) {
            width += 24;
        }
        if (mi->menuItemType == QStyleOptionMenuItem::SubMenu) {
            width += 16;
        }
        return {width, std::max(content.height() + 8, px(T::Size::MenuItemH))};
    }
    case CT_MenuBarItem:
        return {sz.width() + 20, std::max(sz.height() + 8, 28)};
    case CT_TabBarTab: {
        const QSize s = QCommonStyle::sizeFromContents(ct, opt, sz, w);
        return {s.width(), std::max(s.height(), px(T::Size::TabH))};
    }
    case CT_HeaderSection: {
        // File Explorer's column header.
        const QSize s = QCommonStyle::sizeFromContents(ct, opt, sz, w);
        return {s.width() + 8, std::max(opt->fontMetrics.height() + 8, 25)};
    }
    case CT_ItemViewItem: {
        const QSize s = QCommonStyle::sizeFromContents(ct, opt, sz, w);
        return {s.width() + 8, s.height() + 6};
    }
    default:
        break;
    }
    return QCommonStyle::sizeFromContents(ct, opt, sz, w);
}

// KDE convention (Breeze honours it too): kdeglobals [KDE] ShowIconsOnPushButtons, reported by the platform theme.
// Windows dialog buttons carry text only; icon-only buttons keep their icon.
bool Style::hidesButtonIcon(const QStyleOptionButton *b) const
{
    return b && !b->icon.isNull() && !b->text.isEmpty() && !QCommonStyle::styleHint(SH_DialogButtonBox_ButtonsHaveIcons, b, nullptr);
}

// Tabs switching the pages of a QTabWidget (KPageDialog uses document mode too), or a Qt Quick TabBar (no widget).
// Closable or movable tabs are documents (editors, terminals) and keep their own look.
bool Style::isPageTabs(const QStyleOptionTab *tab, const QWidget *w)
{
    if (!w) {
        return !tab || !tab->documentMode;
    }
    const auto *bar = qobject_cast<const QTabBar *>(w);
    return bar && !isTitleTabs(w) && qobject_cast<const QTabWidget *>(w->parentWidget()) && !bar->tabsClosable() && !bar->isMovable();
}

bool Style::isNavigationList(const QStyleOptionViewItem *vi, const QWidget *w)
{
    if (!leadingCell(vi)) {
        return false;
    }
    if (const auto *list = qobject_cast<const QListView *>(w)) {
        // KFilePlacesView paints its icon 4px from the edge, where the pill would sit.
        return list->viewMode() == QListView::ListMode && !w->inherits("KFilePlacesView");
    }
    const auto *tree = qobject_cast<const QTreeView *>(w);
    return tree && tree->isHeaderHidden();
}

QFont Style::groupTitleFont(const QWidget *w)
{
    QFont f = w ? w->font() : QApplication::font();
    f.setWeight(QFont::DemiBold);
    return f;
}

QRect Style::subElementRect(SubElement se, const QStyleOption *opt, const QWidget *w) const
{
    const QRect r = opt->rect;
    switch (se) {
    case SE_HeaderArrow:
        // Above the label, centered in the section.
        return QRect(r.center().x() - 4, r.top(), 9, 5);
    case SE_HeaderLabel: {
        // PM_HeaderMargin pads the sides only: a 25 px Explorer header has no room for it above and below.
        QRect label = QCommonStyle::subElementRect(se, opt, w);
        label.setTop(r.top());
        label.setBottom(r.bottom());
        return label;
    }
    case SE_PushButtonContents:
        return r.adjusted(px(T::Size::ControlPadX), px(T::Size::ControlPadY), -px(T::Size::ControlPadX), -px(T::Size::ControlPadY));
    case SE_PushButtonFocusRect:
    case SE_LineEditContents:
        if (se == SE_LineEditContents && w && (qobject_cast<const QAbstractSpinBox *>(w->parentWidget()) || qobject_cast<const QComboBox *>(w->parentWidget()))) {
            return r.adjusted(0, 1, 0, -1); // the edit field rect of the spin or combo box already holds the text margin
        }
        return se == SE_LineEditContents ? r.adjusted(LineEditTextMargin, 1, -LineEditTextMargin, -1) : r;
    case SE_ProgressBarGroove:
    case SE_ProgressBarContents:
    case SE_ProgressBarLabel: {
        const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(opt);
        const bool horizontal = !pb || (pb->state & State_Horizontal);
        const int label = (pb && pb->textVisible && horizontal) ? opt->fontMetrics.horizontalAdvance(QStringLiteral("100%")) + 8 : 0;
        if (se == SE_ProgressBarLabel) {
            return QRect(r.right() - label + 1, r.top(), label, r.height());
        }
        return horizontal ? r.adjusted(0, 0, -label, 0) : r;
    }
    case SE_TabWidgetTabContents: {
        const auto *tw = qobject_cast<const QTabWidget *>(w);
        if (tw && isPageTabs(nullptr, tw->tabBar())) {
            return QCommonStyle::subElementRect(se, opt, w).adjusted(-1, 3, 1, 1); // no pane frame under tab buttons
        }
        return QCommonStyle::subElementRect(se, opt, w).adjusted(1, 1, -1, -1);
    }
    case SE_ItemViewItemCheckIndicator:
    case SE_ItemViewItemDecoration:
    case SE_ItemViewItemText: {
        QRect rect = QCommonStyle::subElementRect(se, opt, w);
        const auto *vi = qstyleoption_cast<const QStyleOptionViewItem *>(opt);
        if (!vi || !leadingCell(vi) || !rect.isValid()) {
            return rect;
        }
        const bool rtl = vi->direction == Qt::RightToLeft;
        if (se == SE_ItemViewItemText) {
            rtl ? rect.setRight(rect.right() - ItemLead) : rect.setLeft(rect.left() + ItemLead);
        } else {
            rect.translate(rtl ? -ItemLead : ItemLead, 0);
        }
        return rect;
    }
    default:
        return QCommonStyle::subElementRect(se, opt, w);
    }
}

QRect Style::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *w) const
{
    const QRect r = opt->rect;
    switch (cc) {
    case CC_ScrollBar: {
        const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(opt);
        if (!sb) {
            break;
        }
        const bool horizontal = sb->orientation == Qt::Horizontal;
        switch (sc) {
        case SC_ScrollBarAddLine:
        case SC_ScrollBarSubLine:
            return {};
        case SC_ScrollBarGroove:
            return r;
        case SC_ScrollBarSlider:
        case SC_ScrollBarAddPage:
        case SC_ScrollBarSubPage: {
            const int len = horizontal ? r.width() : r.height();
            const int range = sb->maximum - sb->minimum;
            int thumb = range > 0 ? int(qint64(len) * sb->pageStep / (range + sb->pageStep)) : len;
            thumb = std::clamp(thumb, std::min(len, pixelMetric(PM_ScrollBarSliderMin, opt, w)), len);
            const int pos = sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition, len - thumb, sb->upsideDown);
            QRect slider = horizontal ? QRect(r.left() + pos, r.top(), thumb, r.height()) : QRect(r.left(), r.top() + pos, r.width(), thumb);
            if (sc == SC_ScrollBarSlider) {
                return slider;
            }
            if (sc == SC_ScrollBarSubPage) {
                return horizontal ? QRect(r.left(), r.top(), pos, r.height()) : QRect(r.left(), r.top(), r.width(), pos);
            }
            return horizontal ? QRect(slider.right() + 1, r.top(), r.right() - slider.right(), r.height())
                              : QRect(r.left(), slider.bottom() + 1, r.width(), r.bottom() - slider.bottom());
        }
        default:
            break;
        }
        break;
    }
    case CC_ComboBox: {
        switch (sc) {
        case SC_ComboBoxFrame:
        case SC_ComboBoxListBoxPopup:
            return r;
        case SC_ComboBoxArrow:
            return visualRect(opt->direction, r, QRect(r.right() - ArrowArea + 1, r.top(), ArrowArea, r.height()));
        case SC_ComboBoxEditField: {
            const int pad = px(T::Size::ControlPadX);
            return visualRect(opt->direction, r, QRect(r.left() + pad, r.top() + 1, r.width() - pad - ArrowArea, r.height() - 2));
        }
        default:
            break;
        }
        break;
    }
    case CC_SpinBox: {
        const auto *sp = qstyleoption_cast<const QStyleOptionSpinBox *>(opt);
        const bool buttons = sp && sp->buttonSymbols != QAbstractSpinBox::NoButtons;
        const int bw = buttons ? SpinArea : 0;
        switch (sc) {
        case SC_SpinBoxFrame:
            return r;
        case SC_SpinBoxUp:
        case SC_SpinBoxDown: {
            if (!buttons) {
                return {};
            }
            const QRect down(r.right() - 3 - SpinButton, r.top() + 4, SpinButton, r.height() - 8);
            return visualRect(opt->direction, r, sc == SC_SpinBoxDown ? down : down.translated(-SpinButton, 0));
        }
        case SC_SpinBoxEditField:
            // Text margin lives here so Qt Quick spin boxes, which put their text straight into this rect, match widgets.
            return visualRect(opt->direction, r, QRect(r.left() + LineEditTextMargin, r.top() + 1, r.width() - LineEditTextMargin - bw, r.height() - 2));
        default:
            break;
        }
        break;
    }
    case CC_GroupBox: {
        const auto *gb = qstyleoption_cast<const QStyleOptionGroupBox *>(opt);
        if (!gb) {
            break;
        }
        const bool checkable = gb->subControls & SC_GroupBoxCheckBox;
        const int ind = checkable ? pixelMetric(PM_IndicatorWidth, opt, w) : 0;
        const QFontMetrics fm(groupTitleFont(w));
        const int titleH = (checkable || !gb->text.isEmpty()) ? std::max(fm.height(), ind) : 0;
        const int top = r.top() + (titleH ? GroupPad : 0);
        const int head = titleH ? GroupPad + titleH + GroupTitleGap : 0;
        const bool card = isSettingsCard(w);
        switch (sc) {
        case SC_GroupBoxCheckBox:
            return checkable ? visualRect(opt->direction, r, QRect(r.left(), top + (titleH - ind) / 2, ind, ind)) : QRect();
        case SC_GroupBoxLabel: {
            const int x = r.left() + (checkable ? ind + pixelMetric(PM_CheckBoxLabelSpacing, opt, w) : 0);
            return visualRect(opt->direction, r, QRect(x, top, std::min(fm.horizontalAdvance(gb->text) + 2, r.right() - x + 1), titleH));
        }
        case SC_GroupBoxContents:
            if (card) {
                return r.adjusted(CardPad, head + CardPad, -CardPad, -CardPad);
            }
            return titleH ? r.adjusted(0, head, 0, -GroupPad) : r;
        case SC_GroupBoxFrame:
            return card ? r.adjusted(0, head, 0, 0) : r;
        default:
            break;
        }
        break;
    }
    case CC_Slider: {
        // Track, thumb and tick marks share one centre line and one travel span, so the thumb sits on the track at any
        // scale and the value under the cursor matches the thumb (QSlider maps clicks through the groove rect).
        const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(opt);
        if (!sl) {
            break;
        }
        const bool horizontal = sl->orientation == Qt::Horizontal;
        const int d = px(T::Size::SliderThumb);
        const int t = px(T::Size::SliderTrack);
        QRect body = r;
        if (horizontal) {
            body.adjust(0, (sl->tickPosition & QSlider::TicksAbove) ? SliderTickSpace : 0, 0, (sl->tickPosition & QSlider::TicksBelow) ? -SliderTickSpace : 0);
        } else {
            body.adjust((sl->tickPosition & QSlider::TicksLeft) ? SliderTickSpace : 0, 0, (sl->tickPosition & QSlider::TicksRight) ? -SliderTickSpace : 0, 0);
        }
        const int mid = horizontal ? body.top() + body.height() / 2 : body.left() + body.width() / 2;
        switch (sc) {
        case SC_SliderGroove:
            return horizontal ? QRect(r.left(), mid - t / 2, r.width(), t) : QRect(mid - t / 2, r.top(), t, r.height());
        case SC_SliderHandle: {
            const int span = (horizontal ? r.width() : r.height()) - d;
            const int pos = sliderPositionFromValue(sl->minimum, sl->maximum, sl->sliderPosition, span, sl->upsideDown);
            return horizontal ? QRect(r.left() + pos, mid - d / 2, d, d) : QRect(mid - d / 2, r.top() + pos, d, d);
        }
        case SC_SliderTickmarks:
            return r;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return QCommonStyle::subControlRect(cc, opt, sc, w);
}
}
