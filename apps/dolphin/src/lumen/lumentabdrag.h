#pragma once

#include <QByteArray>
#include <QPoint>
#include <QUrl>

#include <optional>

class DolphinTabWidget;
class QMimeData;

namespace Lumen::TabDrag
{
/// A dragged tab: its state (DolphinTabPage::saveState()), the folder of its active view and the drag's origin.
struct Payload {
    QUrl url;
    QByteArray state;
    qint64 pid = 0;
    quint64 id = 0;
};

/// Drags tab `index` out of `tabs` (`grab`: pointer position within the tab). The tab becomes a window following the
/// pointer (xdg-toplevel-drag on Wayland) that docks into any Dolphin window's tabs, also in another process, or opens
/// beside a view in split view; dropped anywhere else it stays a window.
void start(DolphinTabWidget *tabs, int index, const QPoint &grab);

bool carriesTab(const QMimeData *mime);
/// Reads a drag from this process without the offer: Qt would block on our own pipe.
std::optional<Payload> payloadOf(const QMimeData *mime);
/// Drop targets tell the dragging window they would take the tab (it hides so the target shows where the tab lands),
/// and that they took it (only then the dragged window closes: other apps may accept any drop).
void hover(const Payload &payload, bool over);
void take(const Payload &payload);
/// The folder a saved tab state opens first.
QUrl primaryUrlOf(const QByteArray &state);
/// Tabs dropped onto a view open beside it in split view. Once per process.
void installSplitDrop();
}
