pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

// Pinned apps. Paged: rows flip by page (wheel, dots, dragging to an edge). Otherwise "Show all" expands in place.
ColumnLayout {
    id: section

    property bool paged: true
    property int rows: 3
    property bool expanded: false
    property int page: 0
    readonly property alias view: grid
    readonly property int perPage: rows * menu.cols
    readonly property int totalRows: Math.ceil(grid.count / menu.cols)
    readonly property int shownRows: Math.max(1, paged || !expanded ? Math.min(rows, totalRows) : totalRows)
    readonly property int pageCount: paged ? Math.max(1, Math.ceil(grid.count / perPage)) : 1
    signal allApps()

    function reset() {
        page = 0;
        expanded = false;
        grid.currentIndex = -1;
    }

    spacing: 8
    onPageCountChanged: page = Math.min(page, pageCount - 1)

    SectionHeader {
        text: i18n("Pinned")
        action: section.paged ? i18n("All apps") : section.totalRows > section.rows ? (section.expanded ? i18n("Show less") : i18n("Show all")) : ""
        icon: section.paged ? "arrow-right" : section.expanded ? "arrow-up" : "arrow-down"
        onTriggered: section.paged ? section.allApps() : (section.expanded = !section.expanded)
    }

    Item {
        id: area
        Layout.fillWidth: true
        Layout.leftMargin: menu.gridPad
        Layout.rightMargin: menu.gridPad
        Layout.preferredHeight: menu.tileH * section.shownRows

        Item {
            anchors.fill: parent
            clip: true

            GridView {
                id: grid
                readonly property bool pinnable: true
                readonly property int lastIndex: Math.min(count, section.paged ? (section.page + 1) * section.perPage : section.shownRows * menu.cols) - 1

                width: parent.width
                height: Math.max(1, section.totalRows) * menu.tileH
                y: -section.page * section.perPage / menu.cols * menu.tileH
                Behavior on y { NumberAnimation { duration: lt.motionSlowMs; easing.type: lt.easingDecel } }
                interactive: false
                cellWidth: Math.floor(width / menu.cols)
                cellHeight: menu.tileH
                model: start.favorites
                keyNavigationEnabled: true
                currentIndex: -1
                activeFocusOnTab: true
                Accessible.name: i18n("Pinned")
                onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
                onCurrentIndexChanged: if (section.paged && currentIndex >= 0) section.page = Math.floor(currentIndex / section.perPage)
                Component.onCompleted: menu.register(grid)
                Component.onDestruction: menu.unregister(grid)

                Keys.onReturnPressed: start.launch(model, currentIndex)
                Keys.onEnterPressed: start.launch(model, currentIndex)
                Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
                Keys.onDeletePressed: currentItem && start.favorites.removeFavorite(currentItem.model.favoriteId)
                Keys.onPressed: event => {
                    const i = currentIndex;
                    const step = { [Qt.Key_Up]: -menu.cols, [Qt.Key_Down]: menu.cols, [Qt.Key_Left]: -1, [Qt.Key_Right]: 1 }[event.key];
                    if (step === undefined || i < 0) {
                        return;
                    }
                    // Ctrl+Shift+arrows move the pin.
                    if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier)) {
                        if (i + step >= 0 && i + step < count) {
                            start.favorites.moveRow(i, i + step);
                            currentIndex = i + step;
                        }
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Up && i < menu.cols) {
                        menu.step(grid, -1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Down) {
                        const lastRow = section.paged ? section.totalRows - 1 : section.shownRows - 1;
                        if (Math.floor(i / menu.cols) >= lastRow) {
                            menu.step(grid, 1);
                            event.accepted = true;
                        } else if (i + menu.cols >= count) {
                            currentIndex = count - 1;
                            event.accepted = true;
                        }
                    } else if (!section.paged && event.key === Qt.Key_Right && i + 1 > lastIndex) {
                        event.accepted = true;
                    }
                }

                move: Transition {
                    NumberAnimation { properties: "x,y"; duration: lt.motionNormalMs; easing.type: lt.easingDecel }
                }
                moveDisplaced: Transition {
                    NumberAnimation { properties: "x,y"; duration: lt.motionNormalMs; easing.type: lt.easingDecel }
                }
                delegate: AppTile { view: grid }
            }
        }

        PlasmaExtras.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - 48
            visible: grid.count === 0
            text: i18n("Right-click an app and choose “Pin to Start”")
        }

        // Page dots sit in the right margin, like Windows.
        Column {
            anchors { left: parent.right; verticalCenter: parent.verticalCenter }
            visible: section.pageCount > 1
            spacing: 7
            Repeater {
                model: section.pageCount
                delegate: Rectangle {
                    id: dot
                    required property int index
                    readonly property bool current: section.page === index
                    width: 6
                    height: 6
                    radius: 3
                    color: current ? lt.textSecondary : lt.controlStrongFill
                    opacity: current || dotMouse.containsMouse ? 1 : 0.6
                    Behavior on color { ColorAnimation { duration: lt.motionFastMs } }
                    MouseArea {
                        id: dotMouse
                        anchors { fill: parent; margins: -4 }
                        hoverEnabled: true
                        onClicked: section.page = dot.index
                    }
                }
            }
        }

        WheelHandler {
            id: wheel
            property real delta: 0
            enabled: section.pageCount > 1
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => {
                if (cooldown.running) {
                    return;
                }
                delta += event.angleDelta.y;
                if (Math.abs(delta) >= 120) {
                    section.page = Math.max(0, Math.min(section.pageCount - 1, section.page + (delta < 0 ? 1 : -1)));
                    delta = 0;
                    cooldown.restart();
                }
            }
        }
        Timer {
            id: cooldown
            interval: lt.motionSlowMs
        }

        // Reorders live while dragging over the grid; leaving undoes it, so dragging out to the
        // taskbar or desktop keeps the order. Files and launchers dropped here get pinned.
        DropArea {
            id: drop
            property var moves: []
            property int edge: 0
            readonly property Item source: start.dragSource.sourceItem
            anchors.fill: parent
            enabled: Plasmoid.immutability !== PlasmaCore.Types.SystemImmutable

            function own(event): bool {
                return event.source === start.dragSource && drop.source?.view === grid;
            }

            onEntered: moves = []
            onPositionChanged: event => {
                edge = section.pageCount > 1 ? (event.y < 24 ? -1 : event.y > height - 24 ? 1 : 0) : 0;
                if (!own(event)) {
                    return;
                }
                if (!grid.move.running && !grid.moveDisplaced.running) {
                    const p = mapToItem(grid.contentItem, event.x, event.y);
                    const to = grid.indexAt(p.x, p.y);
                    const from = drop.source.index;
                    if (to >= 0 && to !== from) {
                        moves.push([from, to]);
                        start.favorites.moveRow(from, to);
                    }
                }
                event.accept(Qt.MoveAction);
            }
            onExited: {
                edge = 0;
                while (moves.length > 0) {
                    const [from, to] = moves.pop();
                    start.favorites.moveRow(to, from);
                }
            }
            onDropped: event => {
                edge = 0;
                if (own(event)) {
                    moves = [];
                    event.accept(Qt.MoveAction);
                    return;
                }
                if (!event.hasUrls) {
                    return;
                }
                const p = mapToItem(grid.contentItem, event.x, event.y);
                let at = grid.indexAt(p.x, p.y);
                for (const url of event.urls) {
                    start.favorites.addFavorite(url.toString(), at < 0 ? -1 : at++);
                }
                event.accept(Qt.CopyAction);
            }

            Timer {
                interval: 600
                repeat: true
                running: drop.edge !== 0 && drop.containsDrag
                onTriggered: section.page = Math.max(0, Math.min(section.pageCount - 1, section.page + drop.edge))
            }
        }
    }
}
