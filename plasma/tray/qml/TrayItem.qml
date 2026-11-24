pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmaCore.ToolTipArea {
    id: item

    required property int index
    required property var model
    required property int status
    required property int effectiveStatus
    property string itemId: model?.itemId ?? ""
    property bool inOverflow: false
    property string text
    property bool effectivePressed: false
    property int iconBox: root.iconSize
    // Content applets (weather) may draw over the neighbours; their input stays inside this cell.
    property bool spill: false

    readonly property alias iconContainer: iconContainer
    readonly property bool hovered: hover.hovered
    property bool dragging: false

    signal activated(var pos)
    signal clicked(var mouse)
    signal pressed(var mouse)
    signal wheel(var wheel)
    signal contextMenu(var mouse)

    location: Plasmoid.location
    textFormat: Text.AutoText
    opacity: dragging ? 0.4 : 1

    Accessible.name: text
    Accessible.description: subText
    Accessible.role: Accessible.Button

    HoverHandler {
        id: hover
        onHoveredChanged: if (hovered) item.grabDragImage()
    }

    Rectangle {
        id: pill
        anchors.centerIn: parent
        width: parent.width
        height: item.inOverflow ? parent.height : Math.min(parent.height, lt.sizeTaskbarPlate)
        radius: lt.radiusTaskbarButton
        color: iconContainer.pressed ? lt.controlFillPressed : lt.controlFillHover
        border.color: lt.controlStroke
        opacity: item.hovered || iconContainer.pressed || iconContainer.activeFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }

    Rectangle {
        anchors.fill: pill
        radius: lt.radiusControl
        color: "transparent"
        border.width: 2
        border.color: lt.focusOuter
        visible: iconContainer.activeFocus && iconContainer.focusReason === Qt.TabFocusReason
    }

    // Above the icon: the item decides (open popup, menu, forward) instead of the applet's own handlers.
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        z: 2
        acceptedButtons: Qt.AllButtons
        preventStealing: true
        // Dragging (reorder, move between row and overflow) is detected here: a separate handler layer on top
        // would swallow the clicks.
        property point pressPos
        property bool dragStarted: false
        onClicked: mouse => {
            if (!dragStarted) {
                item.clicked(mouse);
            }
        }
        onPressed: mouse => {
            pressPos = Qt.point(mouse.x, mouse.y);
            dragStarted = false;
            item.Drag.active = false;
            item.dragging = false;
            item.hideImmediately();
            item.pressed(mouse);
        }
        onPositionChanged: mouse => {
            if (dragStarted || item.itemId === "" || !(mouse.buttons & Qt.LeftButton)
                || Math.hypot(mouse.x - pressPos.x, mouse.y - pressPos.y) < Application.styleHints.startDragDistance) {
                return;
            }
            dragStarted = true;
            item.dragging = true;
            trayState.dragging = true;
            // Automatic drags start asynchronously; Drag stays active until dragFinished.
            item.Drag.active = true;
        }
        // Long press is the touch context menu; with a mouse (hovering) holding then moving drags instead.
        onPressAndHold: mouse => {
            if (mouse.button === Qt.LeftButton && !hover.hovered) {
                item.contextMenu(mouse);
            }
        }
        onWheel: wheel => {
            item.wheel(wheel);
            wheel.accepted = false;
        }
    }

    FocusScope {
        id: iconContainer
        anchors.centerIn: parent
        width: item.iconBox
        height: item.iconBox
        // Applets drawing inline content (layout switcher) must not spill over neighbours or steal their clicks.
        clip: !item.spill
        focus: true
        activeFocusOnTab: !item.inOverflow
        readonly property bool pressed: item.effectivePressed || mouseArea.containsPress
        readonly property bool inOverflow: item.inOverflow

        scale: pressed ? 0.88 : 1
        Behavior on scale { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }

        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Space:
            case Qt.Key_Enter:
            case Qt.Key_Return:
            case Qt.Key_Select:
                item.activated(Plasmoid.popupPosition(iconContainer, width / 2, height / 2));
                event.accepted = true;
                break;
            case Qt.Key_Menu:
                item.contextMenu(null);
                event.accepted = true;
                break;
            }
        }
    }

    // Attention: accent dot under the icon, gently pulsing.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: iconContainer.bottom
        anchors.topMargin: 3
        width: 4
        height: 4
        radius: 2
        color: lt.accentFill
        visible: item.status === PlasmaCore.Types.NeedsAttentionStatus || item.status === PlasmaCore.Types.RequiresAttentionStatus
        SequentialAnimation on opacity {
            running: parent.visible && Kirigami.Units.longDuration > 0
            loops: Animation.Infinite
            NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 900; easing.type: Easing.InOutSine }
        }
    }

    // Native drag: works between the taskbar row and the overflow flyout (separate windows).
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.MoveAction
    Drag.mimeData: ({ "application/x-lumen-tray-item": item.itemId })
    Drag.hotSpot: Qt.point(width / 2, height / 2)
    Drag.onDragFinished: {
        Drag.active = false;
        dragging = false;
        trayState.dragging = false;
    }

    function grabDragImage() {
        iconContainer.grabToImage(result => item.Drag.imageSource = result.url, Qt.size(root.iconSize * 2, root.iconSize * 2));
    }

}
