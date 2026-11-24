#include "lumentabdrag.h"

#include "dolphinmainwindow.h"
#include "dolphintabpage.h"
#include "dolphintabwidget.h"
#include "dolphinviewcontainer.h"

#include <QApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDataStream>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QTabBar>
#include <QTimer>
#include <QWaylandClientExtension>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

#include <cerrno>
#include <utility>
#include <unistd.h>

namespace Lumen::TabDrag
{
namespace
{
constexpr char MimeType[] = "application/x-lumen-dolphin-tab";

QByteArray encode(const Payload &payload)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << payload.url << payload.state << payload.pid << payload.id;
    return data;
}

std::optional<Payload> decode(const QByteArray &data)
{
    QDataStream stream(data);
    Payload payload;
    stream >> payload.url >> payload.state >> payload.pid >> payload.id;
    if (stream.status() != QDataStream::Ok || payload.state.isEmpty()) {
        return std::nullopt;
    }
    return payload;
}

// xdg-toplevel-drag-v1 (staging): Qt has no wrapper; hand-written like the toplevel tag in lumentitlebar.cpp.
const wl_interface *const noTypes[] = {nullptr, nullptr, nullptr};
const wl_message dragRequests[] = {
    {"destroy", "", const_cast<const wl_interface **>(noTypes)},
    {"attach", "oii", const_cast<const wl_interface **>(noTypes)},
};
const wl_interface toplevelDragInterface = {"xdg_toplevel_drag_v1", 1, 2, dragRequests, 0, nullptr};
const wl_interface *const getDragTypes[] = {&toplevelDragInterface, &wl_data_source_interface};
const wl_message managerRequests[] = {
    {"destroy", "", const_cast<const wl_interface **>(noTypes)},
    {"get_xdg_toplevel_drag", "no", const_cast<const wl_interface **>(getDragTypes)},
};
const wl_interface toplevelDragManagerInterface = {"xdg_toplevel_drag_manager_v1", 1, 2, managerRequests, 0, nullptr};
constexpr uint32_t DragDestroy = 0;
constexpr uint32_t DragAttach = 1;
constexpr uint32_t GetToplevelDrag = 1;

class DataDeviceManager : public QWaylandClientExtension
{
public:
    DataDeviceManager()
        : QWaylandClientExtension(3)
    {
        initialize();
    }
    const wl_interface *extensionInterface() const override
    {
        return &wl_data_device_manager_interface;
    }
    void bind(wl_registry *registry, int id, int version) override
    {
        manager = static_cast<wl_data_device_manager *>(wl_registry_bind(registry, id, &wl_data_device_manager_interface, std::min(version, 3)));
    }
    wl_data_device_manager *manager = nullptr;
};

class ToplevelDragManager : public QWaylandClientExtension
{
public:
    ToplevelDragManager()
        : QWaylandClientExtension(1)
    {
        initialize();
    }
    const wl_interface *extensionInterface() const override
    {
        return &toplevelDragManagerInterface;
    }
    void bind(wl_registry *registry, int id, int version) override
    {
        proxy = static_cast<wl_proxy *>(wl_registry_bind(registry, id, &toplevelDragManagerInterface, std::min(version, 1)));
    }
    wl_proxy *proxy = nullptr;
};

/// The drag in progress in this process (one per seat).
struct Session {
    Payload payload;
    /// The window following the pointer: a new one, or the source window when its only tab is dragged.
    QPointer<DolphinMainWindow> floating;
    /// Pointer position in `floating`.
    QPoint offset;
    wl_data_device *device = nullptr;
    wl_data_source *source = nullptr;
    wl_proxy *drag = nullptr;
    /// A Dolphin drop target (tab strip or view) would take the tab: the window hides meanwhile.
    bool overTarget = false;
    bool hidden = false;
    bool taken = false;
    QTimer retarget;
};
Session *s_session = nullptr;

template<typename T>
T *nativeResource(const char *name, QWidget *window)
{
    auto *native = QGuiApplication::platformNativeInterface();
    QWindow *handle = window ? window->windowHandle() : nullptr;
    return native && handle ? static_cast<T *>(native->nativeResourceForWindow(name, handle)) : nullptr;
}

void attachFloating()
{
    if (auto *toplevel = nativeResource<wl_proxy>("xdg_toplevel", s_session->floating)) {
        wl_proxy_marshal_flags(s_session->drag, DragAttach, nullptr, wl_proxy_get_version(s_session->drag), 0, toplevel, s_session->offset.x(),
                               s_session->offset.y());
    }
}

void applyTarget()
{
    if (!s_session || !s_session->floating || s_session->overTarget == s_session->hidden) {
        return;
    }
    s_session->hidden = s_session->overTarget;
    if (s_session->hidden) {
        s_session->floating->hide();
    } else {
        // Unmapping detached it; attach before the new mapping.
        s_session->floating->show();
        attachFloating();
    }
}

void finish()
{
    Session *s = std::exchange(s_session, nullptr);
    if (!s) {
        return;
    }
    if (s->drag) {
        wl_proxy_marshal_flags(s->drag, DragDestroy, nullptr, wl_proxy_get_version(s->drag), WL_MARSHAL_FLAG_DESTROY);
    }
    if (s->source) {
        wl_data_source_destroy(s->source);
    }
    if (s->device) {
        wl_data_device_release(s->device);
    }
    if (s->floating) {
        if (s->taken) {
            // The tab now lives in the drop target.
            s->floating->close();
        } else if (s->hidden) {
            s->floating->show();
        }
    }
    delete s;
}

// Any client may accept the drop (the Plasma desktop does): Dolphin targets report themselves through hover()/take().
void onTarget(void *, wl_data_source *, const char *)
{
}

void onSend(void *, wl_data_source *, const char *, int32_t fd)
{
    const QByteArray data = s_session ? encode(s_session->payload) : QByteArray();
    for (qsizetype done = 0; done < data.size();) {
        const ssize_t n = ::write(fd, data.constData() + done, data.size() - done);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        done += n;
    }
    ::close(fd);
}

// Ended outside any target (or Escape): the window stays where it was carried.
void onCancelled(void *, wl_data_source *)
{
    QTimer::singleShot(0, &finish);
}

void onDropPerformed(void *, wl_data_source *)
{
}

void onFinished(void *, wl_data_source *)
{
    QTimer::singleShot(0, &finish);
}

void onAction(void *, wl_data_source *, uint32_t)
{
}

const wl_data_source_listener sourceListener = {onTarget, onSend, onCancelled, onDropPerformed, onFinished, onAction};

// Qt's own data device handles the drop side; offers sent to ours are just released.
void onDataOffer(void *, wl_data_device *, wl_data_offer *offer)
{
    wl_data_offer_destroy(offer);
}
void onEnter(void *, wl_data_device *, uint32_t, wl_surface *, wl_fixed_t, wl_fixed_t, wl_data_offer *)
{
}
void onLeave(void *, wl_data_device *)
{
}
void onMotion(void *, wl_data_device *, uint32_t, wl_fixed_t, wl_fixed_t)
{
}
void onDrop(void *, wl_data_device *)
{
}
void onSelection(void *, wl_data_device *, wl_data_offer *)
{
}

const wl_data_device_listener deviceListener = {onDataOffer, onEnter, onLeave, onMotion, onDrop, onSelection};

/// Without xdg-toplevel-drag (X11, other compositors): a plain drag; dropped outside, the tab becomes a window there.
void startFallback(DolphinTabWidget *tabs, DolphinTabPage *page, const Payload &payload, const QPoint &newOffset)
{
    s_session = new Session;
    s_session->payload = payload;
    auto *mime = new QMimeData;
    mime->setData(QLatin1String(MimeType), encode(payload));
    QTabBar *bar = tabs->tabBar();
    auto *drag = new QDrag(bar);
    drag->setMimeData(mime);
    drag->setPixmap(bar->grab(bar->tabRect(tabs->indexOf(page))));
    const QPointer<DolphinTabWidget> tabsAlive(tabs);
    const QPointer<DolphinTabPage> pageAlive(page);
    Qt::DropAction action = drag->exec(Qt::MoveAction);
    if (action == Qt::MoveAction && !s_session->taken) {
        action = Qt::IgnoreAction;
    }
    delete std::exchange(s_session, nullptr);
    if (!tabsAlive || !pageAlive) {
        return;
    }
    QWidget *window = tabs->window();
    if (action == Qt::MoveAction) {
        if (tabs->count() > 1) {
            tabs->takeTab(tabs->indexOf(page));
        } else {
            window->close();
        }
    } else if (action == Qt::IgnoreAction) {
        if (tabs->count() > 1) {
            auto *detached = DolphinMainWindow::openTabWindow(tabs->takeTab(tabs->indexOf(page)), window->size());
            detached->move(QCursor::pos() - newOffset);
            detached->show();
        } else {
            window->move(QCursor::pos() - newOffset);
        }
    }
}

void localHover(quint64 id, bool over)
{
    if (s_session && s_session->payload.id == id) {
        s_session->overTarget = over;
        s_session->retarget.start();
    }
}

void localTake(quint64 id)
{
    if (s_session && s_session->payload.id == id) {
        s_session->taken = true;
    }
}

/// Lets Dolphin windows of other processes report hovering and taking this process's tab drag.
class Endpoint : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.lumen.TabDrag")
public:
    using QObject::QObject;
public Q_SLOTS:
    Q_SCRIPTABLE void hover(qulonglong id, bool over)
    {
        localHover(id, over);
    }
    Q_SCRIPTABLE void take(qulonglong id)
    {
        localTake(id);
    }
};

constexpr char EndpointPath[] = "/lumen/TabDrag";

QDBusMessage endpointCall(const Payload &payload, const char *method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.dolphin-%1").arg(payload.pid), QLatin1String(EndpointPath),
                                          QStringLiteral("org.lumen.TabDrag"), QLatin1String(method));
}

class SplitHint : public QWidget
{
public:
    explicit SplitHint(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor accent = palette().color(QPalette::Highlight);
        QColor fill = accent;
        fill.setAlphaF(0.16);
        p.setBrush(fill);
        accent.setAlphaF(0.9);
        p.setPen(QPen(accent, 2));
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 8, 8);
    }
};

/// Tab drags over a tab page: drop on its left or right half to show the tab's folder there in split view.
class SplitDrop : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const QEvent::Type type = event->type();
        if (type == QEvent::DragLeave) {
            if (watched != m_target) {
                return false;
            }
            m_target = nullptr;
            if (m_hint) {
                m_hint->hide();
            }
            if (m_payload) {
                TabDrag::hover(*m_payload, false);
            }
            // The view never saw the enter we took.
            return true;
        }
        if ((type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::Drop) || !watched->isWidgetType()) {
            return false;
        }
        auto *drop = static_cast<QDropEvent *>(event);
        if (!carriesTab(drop->mimeData())) {
            return false;
        }
        auto *widget = static_cast<QWidget *>(watched);
        DolphinTabPage *page = nullptr;
        for (QWidget *w = widget; w && !page; w = w->parentWidget()) {
            page = qobject_cast<DolphinTabPage *>(w);
        }
        auto *window = page ? qobject_cast<DolphinMainWindow *>(page->window()) : nullptr;
        if (!window) {
            return false;
        }
        const bool left = widget->mapTo(page, drop->position().toPoint()).x() < page->width() / 2;
        drop->setDropAction(Qt::MoveAction);
        drop->accept();
        if (type == QEvent::DragEnter) {
            m_target = widget;
            m_payload = payloadOf(drop->mimeData());
            if (m_payload) {
                TabDrag::hover(*m_payload, true);
            }
        }
        if (type == QEvent::Drop) {
            if (m_hint) {
                m_hint->hide();
            }
            if (const auto payload = payloadOf(drop->mimeData())) {
                take(*payload);
                window->splitWith(payload->url, left);
            }
            m_payload.reset();
            m_target = nullptr;
            return true;
        }
        if (!m_hint || m_hint->parentWidget() != page) {
            delete m_hint;
            m_hint = new SplitHint(page);
        }
        const QRect r = page->rect();
        const int half = r.width() / 2;
        m_hint->setGeometry((left ? QRect(r.left(), r.top(), half, r.height()) : QRect(r.left() + half, r.top(), r.width() - half, r.height())).adjusted(6, 6, -6, -6));
        m_hint->raise();
        m_hint->show();
        return true;
    }

private:
    QPointer<SplitHint> m_hint;
    std::optional<Payload> m_payload;
    QPointer<QWidget> m_target;
};
}

void start(DolphinTabWidget *tabs, int index, const QPoint &grab)
{
    auto *source = qobject_cast<DolphinMainWindow *>(tabs->window());
    if (s_session || !source || index < 0 || index >= tabs->count()) {
        return;
    }
    static const bool registered = QDBusConnection::sessionBus().registerObject(QLatin1String(EndpointPath), new Endpoint(qApp),
                                                                                QDBusConnection::ExportScriptableSlots);
    Q_UNUSED(registered)
    DolphinTabPage *page = tabs->tabPageAt(index);
    const Payload payload{page->activeViewContainer()->url(), page->saveState(), QCoreApplication::applicationPid(), QRandomGenerator::global()->generate64()};
    QTabBar *bar = tabs->tabBar();
    const bool only = tabs->count() == 1;
    // A detached tab is the first one of its window: same strip position, tab rect at the strip's origin.
    const QPoint newOffset = bar->mapTo(source, QPoint()) + grab;
    const QPoint ownOffset = bar->mapTo(source, bar->tabRect(index).topLeft()) + grab;

    const bool wayland = QGuiApplication::platformName() == QLatin1String("wayland");
    static DataDeviceManager *const devices = wayland ? new DataDeviceManager : nullptr;
    static ToplevelDragManager *const drags = wayland ? new ToplevelDragManager : nullptr;
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    auto *surface = nativeResource<wl_surface>("surface", source);
    if (!wayland || !app || !app->lastInputSeat() || !surface || !devices->manager || !drags->proxy) {
        startFallback(tabs, page, payload, only ? ownOffset : newOffset);
        return;
    }

    auto *s = new Session;
    s_session = s;
    s->payload = payload;
    s->retarget.setSingleShot(true);
    s->retarget.setInterval(40);
    QObject::connect(&s->retarget, &QTimer::timeout, &applyTarget);
    s->device = wl_data_device_manager_get_data_device(devices->manager, app->lastInputSeat());
    wl_data_device_add_listener(s->device, &deviceListener, nullptr);
    s->source = wl_data_device_manager_create_data_source(devices->manager);
    wl_data_source_offer(s->source, MimeType);
    wl_data_source_set_actions(s->source, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
    wl_data_source_add_listener(s->source, &sourceListener, nullptr);
    s->drag = static_cast<wl_proxy *>(wl_proxy_marshal_flags(drags->proxy, GetToplevelDrag, &toplevelDragInterface, wl_proxy_get_version(drags->proxy), 0,
                                                             nullptr, s->source));
    const uint32_t serial = app->lastInputSerial();
    if (only) {
        // The whole window moves; an existing toplevel is attached before the drag starts.
        s->floating = source;
        s->offset = ownOffset;
        attachFloating();
        wl_data_device_start_drag(s->device, s->source, surface, nullptr, serial);
        return;
    }
    wl_data_device_start_drag(s->device, s->source, surface, nullptr, serial);
    s->floating = DolphinMainWindow::openTabWindow(tabs->takeTab(index), source->size());
    s->offset = newOffset;
    s->floating->show();
    attachFloating();
}

void hover(const Payload &payload, bool over)
{
    if (payload.pid == QCoreApplication::applicationPid()) {
        localHover(payload.id, over);
    } else {
        QDBusConnection::sessionBus().asyncCall(endpointCall(payload, "hover") << qulonglong(payload.id) << over);
    }
}

void take(const Payload &payload)
{
    if (payload.pid == QCoreApplication::applicationPid()) {
        localTake(payload.id);
    } else {
        // Blocking: the source has to know before the drop finishes.
        QDBusConnection::sessionBus().call(endpointCall(payload, "take") << qulonglong(payload.id), QDBus::Block, 500);
    }
}

bool carriesTab(const QMimeData *mime)
{
    return mime && mime->hasFormat(QLatin1String(MimeType));
}

std::optional<Payload> payloadOf(const QMimeData *mime)
{
    if (!carriesTab(mime)) {
        return std::nullopt;
    }
    if (s_session) {
        return s_session->payload;
    }
    return decode(mime->data(QLatin1String(MimeType)));
}

QUrl primaryUrlOf(const QByteArray &state)
{
    QDataStream stream(state);
    quint32 version = 0;
    bool split = false;
    QUrl url;
    stream >> version >> split >> url;
    return url;
}

void installSplitDrop()
{
    static bool installed = false;
    if (!installed) {
        installed = true;
        qApp->installEventFilter(new SplitDrop(qApp));
    }
}
}

#include "lumentabdrag.moc"
