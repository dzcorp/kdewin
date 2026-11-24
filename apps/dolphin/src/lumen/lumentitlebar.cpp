#include "lumentitlebar.h"

#include "lumencolor.h"
#include "lumenglyph.h"
#include "lumentabdrag.h"

#include <KLocalizedString>

#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QGuiApplication>
#include <QPainter>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QWaylandClientExtension>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

namespace Lumen
{
namespace
{
// xdg-toplevel-tag-v1 (staging): Qt has no wrapper, and the request only needs the toplevel proxy and a string.
const wl_interface *const tagTypes[] = {nullptr, nullptr};
const wl_message tagRequests[] = {
    {"destroy", "", const_cast<const wl_interface **>(tagTypes)},
    {"set_toplevel_tag", "os", const_cast<const wl_interface **>(tagTypes)},
    {"set_toplevel_description", "os", const_cast<const wl_interface **>(tagTypes)},
};
const wl_interface tagManagerInterface = {"xdg_toplevel_tag_manager_v1", 1, 3, tagRequests, 0, nullptr};
constexpr uint32_t SetToplevelTag = 1;
constexpr uint32_t ShowWindowMenu = 4; // xdg_toplevel.show_window_menu

class TagManager : public QWaylandClientExtension
{
public:
    TagManager()
        : QWaylandClientExtension(1)
    {
        initialize();
    }
    const wl_interface *extensionInterface() const override
    {
        return &tagManagerInterface;
    }
    void bind(wl_registry *registry, int id, int version) override
    {
        m_proxy = static_cast<wl_proxy *>(wl_registry_bind(registry, id, &tagManagerInterface, std::min(version, 1)));
    }
    void tag(wl_proxy *toplevel, const char *tag) const
    {
        if (m_proxy) {
            wl_proxy_marshal_flags(m_proxy, SetToplevelTag, nullptr, wl_proxy_get_version(m_proxy), 0, toplevel, tag);
        }
    }

private:
    wl_proxy *m_proxy = nullptr;
};

wl_proxy *toplevelOf(QWindow *window)
{
    auto *native = QGuiApplication::platformNativeInterface();
    return native ? static_cast<wl_proxy *>(native->nativeResourceForWindow("xdg_toplevel", window)) : nullptr;
}

// Qt creates the xdg_toplevel only on first expose after each show; KWin re-evaluates its rules when the tag arrives.
class WindowTagger : public QObject
{
public:
    explicit WindowTagger(QWindow *window)
        : QObject(window)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        static TagManager *const manager = new TagManager;
        if (event->type() == QEvent::Hide) {
            m_tagged = false;
        } else if (event->type() == QEvent::Expose && !m_tagged && manager->isActive()) {
            if (wl_proxy *toplevel = toplevelOf(static_cast<QWindow *>(watched))) {
                manager->tag(toplevel, "org.lumen.titlebar");
                m_tagged = true;
            }
        }
        return false;
    }

private:
    bool m_tagged = false;
};
}

CaptionButton::CaptionButton(Caption kind, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
}

QSize CaptionButton::sizeHint() const
{
    return {int(T::Size::CaptionButtonW), int(T::Size::CaptionButtonH)};
}

void CaptionButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const auto &f = fluent(palette());
    const bool close = m_kind == Caption::Close;
    QColor glyph = window()->isActiveWindow() ? palette().color(QPalette::WindowText) : argb(f.textTertiary);
    if (isDown()) {
        p.fillRect(rect(), argb(close ? f.closePressed : f.subtleFillPressed));
        glyph = close ? withAlpha(argb(f.closeGlyph), 0.7) : withAlpha(glyph, 0.75);
    } else if (underMouse()) {
        p.fillRect(rect(), argb(close ? f.closeHover : f.subtleFillHover));
        if (close) {
            glyph = argb(f.closeGlyph);
        }
    }
    const Caption kind = m_kind == Caption::Maximize && window()->isMaximized() ? Caption::Restore : m_kind;
    const qreal g = T::Size::CaptionGlyph;
    const QRectF box(QRectF(rect()).center() - QPointF(g / 2, g / 2), QSizeF(g, g));
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(captionGlyph(kind, box, devicePixelRatioF()), glyph);
}

TitleBar::TitleBar(QTabBar *tabs, QWidget *parent)
    : QWidget(parent)
    , m_tabs(tabs)
    , m_newTab(new QToolButton(this))
    , m_sidebar(new QToolButton(this))
    , m_maximize(new CaptionButton(Caption::Maximize, this))
{
    setFixedHeight(Height);
    setAcceptDrops(true);
    tabs->setProperty("_lumen_titletabs", true);
    tabs->setDrawBase(false);
    tabs->setExpanding(false);

    m_newTab->setAutoRaise(true);
    m_newTab->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_newTab->setToolTip(i18nc("@action:inmenu", "New Tab"));
    m_newTab->setFixedSize(36, 30);
    m_sidebar->setAutoRaise(true);
    m_sidebar->setIcon(glyphIcon(Glyph::Sidebar));
    m_sidebar->setIconSize(QSize(22, 22));
    m_sidebar->setFixedSize(36, 30);

    auto *minimize = new CaptionButton(Caption::Minimize, this);
    auto *close = new CaptionButton(Caption::Close, this);
    connect(minimize, &QAbstractButton::clicked, this, [this] {
        window()->showMinimized();
    });
    connect(m_maximize, &QAbstractButton::clicked, this, &TitleBar::toggleMaximized);
    connect(close, &QAbstractButton::clicked, this, [this] {
        window()->close();
    });

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 0, 0, 0);
    row->setSpacing(2);
    row->addWidget(m_sidebar, 0, Qt::AlignBottom);
    row->addWidget(tabs, 0, Qt::AlignBottom);
    row->addWidget(m_newTab, 0, Qt::AlignBottom);
    row->addStretch(1);
    row->setSpacing(0);
    for (auto *b : {minimize, m_maximize, close}) {
        row->addWidget(b, 0, Qt::AlignTop);
    }
    parent->window()->installEventFilter(this);
}

QToolButton *TitleBar::newTabButton() const
{
    return m_newTab;
}

QToolButton *TitleBar::sidebarButton() const
{
    return m_sidebar;
}

void TitleBar::tagWindow(QWidget *window)
{
    QWindow *handle = window->windowHandle();
    if (!handle || QGuiApplication::platformName() != QLatin1String("wayland")) {
        return;
    }
    handle->installEventFilter(new WindowTagger(handle));
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::ActivationChange) {
        for (auto *b : findChildren<CaptionButton *>()) {
            b->update();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TitleBar::forwardDrop(QDropEvent *event)
{
    if (!TabDrag::carriesTab(event->mimeData())) {
        event->ignore();
        return;
    }
    // Straight to the tab bar's handler: through QApplication an ignored drag would bubble back up to us.
    const QPoint end(m_tabs->width() - 1, m_tabs->height() / 2);
    const auto deliver = [&](QDropEvent &forwarded) {
        static_cast<QObject *>(m_tabs)->event(&forwarded);
        event->setDropAction(forwarded.dropAction());
        event->setAccepted(forwarded.isAccepted());
    };
    if (event->type() == QEvent::Drop) {
        QDropEvent forwarded(end, event->possibleActions(), event->mimeData(), event->buttons(), event->modifiers());
        deliver(forwarded);
    } else if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent forwarded(end, event->possibleActions(), event->mimeData(), event->buttons(), event->modifiers());
        deliver(forwarded);
    } else {
        QDragMoveEvent forwarded(end, event->possibleActions(), event->mimeData(), event->buttons(), event->modifiers());
        deliver(forwarded);
    }
}

void TitleBar::dragEnterEvent(QDragEnterEvent *event)
{
    forwardDrop(event);
}

void TitleBar::dragMoveEvent(QDragMoveEvent *event)
{
    forwardDrop(event);
}

void TitleBar::dragLeaveEvent(QDragLeaveEvent *)
{
    QDragLeaveEvent forwarded;
    static_cast<QObject *>(m_tabs)->event(&forwarded);
}

void TitleBar::dropEvent(QDropEvent *event)
{
    forwardDrop(event);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        toggleMaximized();
    }
}

void TitleBar::contextMenuEvent(QContextMenuEvent *event)
{
    auto *wl = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    wl_proxy *toplevel = window()->windowHandle() ? toplevelOf(window()->windowHandle()) : nullptr;
    if (!wl || !toplevel || !wl->lastInputSeat()) {
        return;
    }
    const QPoint pos = mapTo(window(), event->pos());
    wl_proxy_marshal_flags(toplevel, ShowWindowMenu, nullptr, wl_proxy_get_version(toplevel), 0, wl->lastInputSeat(), wl->lastInputSerial(), pos.x(), pos.y());
}

void TitleBar::toggleMaximized()
{
    if (window()->isMaximized()) {
        window()->showNormal();
    } else {
        window()->showMaximized();
    }
}
}
