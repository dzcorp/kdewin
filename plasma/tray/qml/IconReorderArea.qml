pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: area

    required property var items
    required property var order
    required property color indicatorColor
    readonly property alias containsMouse: mouse.containsMouse
    readonly property alias containsPress: mouse.containsPress
    property bool dragging: false
    property string draggedId
    property int insertAt: -1

    signal pressed(var event)
    signal clicked(var event)
    signal wheel(var event)
    signal orderRequested(var order)

    function itemAt(x: real): var {
        if (items.length === 0) {
            return null;
        }
        for (let i = 0; i < items.length - 1; ++i) {
            const a = items[i].mapToItem(area, items[i].width, 0).x;
            const b = items[i + 1].mapToItem(area, 0, 0).x;
            if (x < (a + b) / 2) {
                return items[i];
            }
        }
        return items[items.length - 1];
    }

    function indexAt(x: real): int {
        for (let i = 0; i < items.length; ++i) {
            if (x < items[i].mapToItem(area, items[i].width / 2, 0).x) {
                return i;
            }
        }
        return items.length;
    }

    function containsPoint(x: real, y: real): bool {
        return x >= 0 && x <= width && y >= 0 && y <= height;
    }

    function moveBefore(id: string, before: string) {
        if (id === before || order.indexOf(id) < 0) {
            return;
        }
        const next = order.filter(item => item !== id);
        const at = next.indexOf(before);
        next.splice(at < 0 ? next.length : at, 0, id);
        orderRequested(next);
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        property point pressPos
        property bool dragStarted: false

        onPressed: event => {
            pressPos = Qt.point(event.x, event.y);
            dragStarted = false;
            area.insertAt = -1;
            const item = area.itemAt(event.x);
            area.draggedId = item?.itemId ?? "";
            area.pressed(event);
        }
        onPositionChanged: event => {
            if (area.draggedId === "" || !(event.buttons & Qt.LeftButton)) {
                return;
            }
            if (!dragStarted) {
                if (Math.hypot(event.x - pressPos.x, event.y - pressPos.y) < Application.styleHints.startDragDistance) {
                    return;
                }
                dragStarted = true;
                area.dragging = true;
            }
            area.insertAt = area.containsPoint(event.x, event.y) ? area.indexAt(event.x) : -1;
        }
        onReleased: event => {
            if (dragStarted && area.containsPoint(event.x, event.y)) {
                const before = area.items[area.indexAt(event.x)]?.itemId ?? "";
                area.moveBefore(area.draggedId, before);
            }
            area.dragging = false;
            area.insertAt = -1;
        }
        onCanceled: {
            area.dragging = false;
            area.insertAt = -1;
        }
        onClicked: event => {
            if (!dragStarted) {
                area.clicked(event);
            }
        }
        onWheel: event => area.wheel(event)
    }

    Rectangle {
        visible: area.dragging && area.insertAt >= 0
        color: area.indicatorColor
        width: 2
        height: Math.min(area.height, (area.items[0]?.height ?? 16) + 8)
        radius: 1
        y: (area.height - height) / 2
        x: {
            if (area.items.length === 0 || area.insertAt < 0) {
                return 0;
            }
            const index = area.insertAt;
            if (index === 0) {
                return area.items[0].mapToItem(area, 0, 0).x - 4;
            }
            const previous = area.items[index - 1];
            const left = previous.mapToItem(area, previous.width, 0).x;
            return index === area.items.length ? left + 4
                : (left + area.items[index].mapToItem(area, 0, 0).x) / 2 - 1;
        }
    }
}
