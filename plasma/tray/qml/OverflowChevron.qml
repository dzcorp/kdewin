import QtQuick
import QtQuick.Shapes
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmaCore.ToolTipArea {
    id: chevron

    readonly property bool open: trayState.overflowOpen
    readonly property bool upward: Plasmoid.location !== PlasmaCore.Types.TopEdge

    mainText: open ? i18n("Hide hidden icons") : i18n("Show hidden icons")
    location: Plasmoid.location
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: mainText
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            trayState.overflowOpen = !trayState.overflowOpen;
            event.accepted = true;
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: Math.min(parent.height, lt.sizeTaskbarPlate)
        radius: lt.radiusTaskbarButton
        color: mouse.pressed ? lt.controlFillPressed : lt.controlFillHover
        border.color: lt.controlStroke
        opacity: mouse.containsMouse || chevron.open || drop.containsDrag ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }

    Shape {
        anchors.centerIn: parent
        width: 10
        height: 5
        preferredRendererType: Shape.CurveRenderer
        rotation: (chevron.open !== chevron.upward) ? 0 : 180
        Behavior on rotation { NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel } }
        ShapePath {
            strokeColor: lt.textPrimary
            strokeWidth: 1.25
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            startX: 0.5; startY: 4.5
            PathLine { x: 5; y: 0.5 }
            PathLine { x: 9.5; y: 4.5 }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        // The flyout hides itself on the press (focus loss): decide from the state before it.
        property bool wasOpen: false
        onPressed: wasOpen = trayState.overflowOpen
        onClicked: trayState.overflowOpen = !wasOpen
    }

    // Dragging an icon over the chevron opens the flyout; dropping on it hides the icon there.
    DropArea {
        id: drop
        anchors.fill: parent
        keys: [root.dragMime]
        onEntered: openTimer.restart()
        onExited: openTimer.stop()
        onDropped: event => {
            openTimer.stop();
            const id = root.dragId(event);
            if (id !== "") {
                Plasmoid.setItemVisibility(id, 2);
                event.accept(Qt.MoveAction);
            }
        }
        Timer {
            id: openTimer
            interval: 400
            onTriggered: trayState.overflowOpen = true
        }
    }
}
