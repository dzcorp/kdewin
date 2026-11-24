pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// Win11-style hidden icons flyout: compact grid, drag to sort, drag out to pin to the taskbar.
PlasmaCore.PopupPlasmaWindow {
    id: flyout

    readonly property int cell: 40
    readonly property int pad: 2 // plus the 12 px dialog margin
    readonly property int columns: Math.max(1, Math.min(5, grid.count))

    // PlasmaWindow does not size itself from its main item.
    width: content.implicitWidth + leftPadding + rightPadding
    height: content.implicitHeight + topPadding + bottomPadding
    visible: trayState.overflowOpen
    popupDirection: switch (Plasmoid.location) {
        case PlasmaCore.Types.TopEdge: return Qt.BottomEdge;
        case PlasmaCore.Types.LeftEdge: return Qt.RightEdge;
        case PlasmaCore.Types.RightEdge: return Qt.LeftEdge;
        default: return Qt.TopEdge;
    }
    margin: lt.sizeFlyoutPad
    removeBorderStrategy: PlasmaCore.PopupPlasmaWindow.Never
    hideOnWindowDeactivate: !trayState.dragging
    animated: true

    onVisibleChanged: {
        if (visible) {
            requestActivate();
            grid.forceActiveFocus();
        } else {
            closeSync.restart();
        }
    }

    // Opened by dragging over the chevron: take focus afterwards so clicking elsewhere still closes it.
    Connections {
        target: trayState
        function onDraggingChanged() {
            if (!trayState.dragging && flyout.visible) {
                flyout.requestActivate();
            }
        }
    }

    Timer {
        id: closeSync
        interval: 100
        onTriggered: trayState.overflowOpen = flyout.visible
    }

    mainItem: FocusScope {
        id: content
        implicitWidth: (grid.count > 0 ? flyout.columns * flyout.cell : flyout.cell * 3) + flyout.pad * 2
        implicitHeight: Math.max(1, Math.ceil(grid.count / flyout.columns)) * flyout.cell + flyout.pad * 2
        Keys.onEscapePressed: trayState.overflowOpen = false

        PlasmaComponents3.Label {
            anchors.centerIn: parent
            visible: grid.count === 0
            text: i18n("Drop icons here")
            color: lt.textSecondary
            font.pixelSize: lt.typeCaption[0]
        }

        GridView {
            id: grid
            anchors.fill: parent
            anchors.margins: flyout.pad
            cellWidth: flyout.cell
            cellHeight: flyout.cell
            interactive: false
            keyNavigationEnabled: true
            currentIndex: -1
            model: overflowModel
            delegate: ItemLoader {
                inOverflow: true
                width: flyout.cell
                height: flyout.cell
            }
            highlight: Rectangle {
                radius: lt.radiusControl
                color: lt.subtleFillHover
            }
            highlightMoveDuration: 0
            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: lt.motionNormalMs; easing.type: lt.easingDecel }
            }
            onActiveFocusChanged: currentIndex = activeFocus && count > 0 ? Math.max(0, currentIndex) : -1
            Keys.onPressed: event => {
                const target = currentItem?.item;
                if (!target) {
                    return;
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                    target.activated(Plasmoid.popupPosition(currentItem, flyout.cell / 2, flyout.cell / 2));
                    event.accepted = true;
                } else if (event.key === Qt.Key_Menu) {
                    target.contextMenu(null);
                    event.accepted = true;
                }
            }
        }

        DropArea {
            id: drop
            anchors.fill: parent
            keys: [root.dragMime]
            property int insertAt: -1

            function indexAt(event): int {
                const col = Math.max(0, Math.min(flyout.columns, Math.round((event.x - flyout.pad) / flyout.cell)));
                const row = Math.max(0, Math.floor((event.y - flyout.pad) / flyout.cell));
                return Math.min(grid.count, row * flyout.columns + col);
            }
            onPositionChanged: event => insertAt = indexAt(event)
            onExited: insertAt = -1
            onDropped: event => {
                const id = root.dragId(event);
                if (id !== "") {
                    const at = indexAt(event);
                    const before = at < grid.count ? (grid.itemAtIndex(at)?.itemId ?? "") : "";
                    Plasmoid.setItemVisibility(id, 2);
                    Plasmoid.moveItem(id, before);
                    event.accept(Qt.MoveAction);
                }
                insertAt = -1;
            }
        }

        Rectangle {
            visible: drop.containsDrag && drop.insertAt >= 0
            color: lt.accentFill
            width: 2
            height: root.iconSize + 8
            radius: 1
            x: flyout.pad + (drop.insertAt % flyout.columns) * flyout.cell - 1
            y: flyout.pad + Math.floor(drop.insertAt / flyout.columns) * flyout.cell + (flyout.cell - height) / 2
        }
    }
}
