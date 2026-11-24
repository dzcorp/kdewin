pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// Network / volume / battery icons as one button (the merged applets' own compact representations).
Item {
    id: button

    readonly property var alwaysVisible: ["org.kde.plasma.networkmanagement", "org.kde.plasma.volume"]
    readonly property bool open: trayState.expanded && (!trayState.showingApplet || trayState.fromQuickSettings)

    implicitWidth: icons.implicitWidth + root.spacing
    implicitHeight: icons.implicitHeight + root.spacing
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: i18n("Quick Settings")
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            trayState.toggleQuickSettings();
            event.accepted = true;
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: Math.min(parent.height, lt.sizeTaskbarPlate)
        radius: lt.radiusTaskbarButton
        color: mouse.containsPress ? lt.controlFillPressed : lt.controlFillHover
        border.color: lt.controlStroke
        opacity: mouse.containsMouse || button.open || button.activeFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }

    // Slots follow quickSettingsItems order; the tray model keeps its own.
    Item {
        id: icons
        anchors.centerIn: parent

        readonly property int pitch: root.quickIconSize + root.spacing - 4
        property int revision: 0
        readonly property var order: {
            revision;
            const shown = [];
            for (let i = 0; i < slots.count; ++i) {
                const s = slots.itemAt(i);
                if (s?.shown) {
                    shown.push(s);
                }
            }
            return shown.sort((a, b) => a.rank - b.rank);
        }

        implicitWidth: Math.max(0, order.length * pitch - (pitch - root.quickIconSize))
        implicitHeight: root.quickIconSize

        Repeater {
            id: slots
            model: quickSettingsModel
            onItemAdded: icons.revision++
            onItemRemoved: icons.revision++
            delegate: Item {
                id: slot
                required property var model
                readonly property string itemId: model.itemId
                readonly property Item applet: model.applet ?? null
                readonly property int rank: root.quickSettingsIds.indexOf(model.itemId)
                readonly property bool inOverflow: false
                readonly property bool shown: applet !== null && model.status !== PlasmaCore.Types.HiddenStatus
                    && (model.status !== PlasmaCore.Types.PassiveStatus || button.alwaysVisible.indexOf(model.itemId) >= 0)
                visible: shown
                opacity: mouse.dragging && mouse.draggedId === itemId ? 0.4 : 1
                clip: true
                x: Math.max(0, icons.order.indexOf(slot)) * icons.pitch
                width: root.quickIconSize
                height: root.quickIconSize

                StatusGlyph {
                    id: glyph
                    anchors.fill: parent
                    z: 1
                    iconName: slot.applet?.plasmoid?.icon ?? ""
                }
                // No restore: a recreated delegate's Binding applies first, then the old one would undo it.
                Binding {
                    target: slot.applet
                    when: slot.applet !== null
                    property: "opacity"
                    value: glyph.spec ? 0 : 1
                    restoreMode: Binding.RestoreNone
                }
                function adopt() {
                    if (applet) {
                        applet.parent = slot;
                        applet.anchors.fill = slot;
                        applet.visible = true;
                        root.fitIcons(applet);
                        exactTimer.restart();
                    }
                }
                Timer {
                    id: exactTimer
                    interval: 300
                    onTriggered: root.exactIcons(slot.applet)
                }
                onWidthChanged: {
                    root.fitIcons(slot.applet);
                    exactTimer.restart();
                }
                // Change handlers skip the initial value: adopt on creation too.
                onAppletChanged: adopt()
                Component.onCompleted: adopt()
            }
        }
    }

    function appletAt(x: real): Item {
        const p = mapToItem(icons, x, height / 2);
        return icons.childAt(p.x, icons.height / 2)?.applet ?? root.appletItemFor("org.kde.plasma.volume");
    }

    IconReorderArea {
        id: mouse
        anchors.fill: parent
        z: 10
        items: icons.order
        order: Plasmoid.configuration.quickSettingsItems
        indicatorColor: lt.accentFill
        onDraggingChanged: trayState.dragging = dragging
        onOrderRequested: order => Plasmoid.configuration.quickSettingsItems = order
        property bool wasOpen: false
        onPressed: wasOpen = button.open
        onClicked: event => {
            if (event.button === Qt.LeftButton) {
                if (wasOpen) {
                    trayState.expanded = false;
                } else {
                    trayState.showQuickSettings();
                }
                return;
            }
            const applet = button.appletAt(event.x);
            if (event.button === Qt.RightButton) {
                if (applet) {
                    Plasmoid.showPlasmoidMenu(applet, 0, 0);
                }
            } else {
                root.appletMouseArea(applet)?.clicked(event);
            }
        }
        onWheel: wheel => root.appletMouseArea(button.appletAt(wheel.x))?.wheel(wheel)
    }
}
