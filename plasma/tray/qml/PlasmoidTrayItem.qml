pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

TrayItem {
    id: container

    property Item applet: model?.applet ?? null
    // Icon applets get their icon fitted by its artwork, like app icons; other content (weather) a stock-sized box.
    property Item fitIcon: null
    property bool content: false
    readonly property rect fit: fitIcon ? Plasmoid.iconPlacement(fitIcon.source, iconBox) : Qt.rect(0, 0, iconBox, iconBox)
    iconBox: content ? Math.min(root.cell, Math.round(root.iconSize * 1.75)) : root.iconSize
    spill: content

    function classify() {
        const rep = applet?.compactRepresentationItem ?? null;
        const inline = applet?.fullRepresentationItem && applet.fullRepresentationItem.parent === applet;
        let best = null;
        let text = false;
        const walk = item => {
            for (const c of item.children) {
                if (!c.visible || c.opacity === 0) {
                    continue;
                }
                if (c.roundToIconSize !== undefined && c.placeholder !== undefined && c.source && (!best || c.width * c.height > best.width * best.height)) {
                    best = c;
                } else if (typeof c.text === "string" && c.text !== "" && c.font !== undefined && c.width > 0) {
                    text = true;
                }
                walk(c);
            }
        };
        if (rep && !inline) {
            walk(rep);
        }
        content = text;
        fitIcon = !text && best !== null && best.width >= rep.width * 0.6 && best.height >= rep.height * 0.6 ? best : null;
    }

    text: applet?.Plasmoid.title ?? ""
    itemId: applet?.Plasmoid.pluginName ?? ""
    // CompactApplet draws the applet's own tooltip.
    active: false
    mainText: ""
    subText: ""

    function appletMouseArea(): MouseArea {
        return root.appletMouseArea(applet);
    }

    onActivated: pos => applet?.Plasmoid.activated()
    // Popup state at press time: the popup already hides on the press (focus loss), so the click must not reopen it.
    property bool wasOpen: false

    onClicked: mouse => {
        if (!applet) {
            return;
        }
        // Applets showing their content inline (keyboard layout switcher) handle clicks themselves.
        if (applet.fullRepresentationItem && applet.fullRepresentationItem.parent === applet) {
            appletMouseArea()?.clicked(mouse);
            return;
        }
        if (mouse.button === Qt.LeftButton) {
            if (wasOpen) {
                trayState.expanded = false;
            } else if (applet.expanded) {
                trayState.openApplet(applet, false);
            } else {
                applet.expanded = true;
            }
        } else if (mouse.button === Qt.MiddleButton) {
            appletMouseArea()?.clicked(mouse);
        }
    }
    onPressed: mouse => {
        wasOpen = trayState.expanded && trayState.activeApplet === applet;
        if (mouse.button === Qt.RightButton) {
            contextMenu(mouse);
        }
    }
    onContextMenu: mouse => {
        if (applet) {
            effectivePressed = false;
            Plasmoid.showPlasmoidMenu(applet, 0, 0);
        }
    }
    onWheel: wheel => appletMouseArea()?.wheel(wheel)

    function adopt() {
        if (applet) {
            applet.parent = iconContainer;
            applet.anchors.fill = undefined;
            applet.visible = true;
            applet.opacity = 1;
            root.fitIcons(applet);
            exactTimer.restart();
        }
    }
    // Compact representations are often built lazily: fix icon snapping once they exist and after resizes.
    Timer {
        id: exactTimer
        interval: 300
        onTriggered: {
            root.exactIcons(container.applet);
            container.classify();
        }
    }
    Connections {
        target: container.applet
        function onCompactRepresentationItemChanged() {
            exactTimer.restart();
        }
    }
    Binding {
        target: container.applet
        when: container.applet !== null
        property: "x"
        value: container.fit.x
    }
    Binding {
        target: container.applet
        when: container.applet !== null
        property: "y"
        value: container.fit.y
    }
    Binding {
        target: container.applet
        when: container.applet !== null
        property: "width"
        value: container.fit.width
    }
    Binding {
        target: container.applet
        when: container.applet !== null
        property: "height"
        value: container.fit.height
    }
    Connections {
        target: container.iconContainer
        function onWidthChanged() {
            root.fitIcons(container.applet);
            exactTimer.restart();
        }
    }
    onAppletChanged: adopt()
    Component.onCompleted: adopt()

    Connections {
        enabled: container.applet !== null
        target: container.appletMouseArea()
        function onContainsPressChanged() {
            container.effectivePressed = (target as MouseArea).containsPress;
        }
        function onPressAndHold(mouse) {
            if (mouse.button === Qt.LeftButton) {
                container.contextMenu(mouse);
            }
        }
    }

    Connections {
        target: container.applet?.Plasmoid ?? null
        function onActivated() {
            container.effectivePressed = true;
            Qt.callLater(() => container.effectivePressed = false);
        }
    }

    PlasmaComponents3.BusyIndicator {
        anchors.fill: parent
        z: 999
        running: container.applet?.Plasmoid.busy ?? false
        visible: running
    }

    Binding {
        target: container.applet
        when: container.applet !== null
        property: "hideOnWindowDeactivate"
        value: !Plasmoid.configuration.pin
        restoreMode: Binding.RestoreBinding
    }
}
