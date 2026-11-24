import QtQuick

// One popup hosts Quick Settings and applet pages; the overflow flyout is separate.
QtObject {
    id: state

    property bool expanded: false
    property bool overflowOpen: false
    // An icon is being dragged: popups must not hide on focus changes meanwhile.
    property bool dragging: false
    property Item activeApplet: null
    property bool fromQuickSettings: false
    // Popup anchor: the icon in the taskbar row, else the Quick Settings button.
    property Item anchorItem: null
    readonly property bool showingApplet: activeApplet !== null

    function showQuickSettings() {
        overflowOpen = false;
        closeActiveApplet();
        anchorItem = null;
        expanded = true;
    }

    function toggleQuickSettings() {
        if (expanded && !showingApplet) {
            expanded = false;
        } else {
            showQuickSettings();
        }
    }

    function openApplet(applet, viaQuickSettings) {
        if (!applet || applet === activeApplet || applet.preferredRepresentation === applet.fullRepresentation) {
            return;
        }
        overflowOpen = false;
        fromQuickSettings = !!viaQuickSettings;
        const host = applet.parent;
        anchorItem = !fromQuickSettings && host && host.inOverflow === false ? host : null;
        const old = activeApplet;
        activeApplet = applet;
        if (old) {
            old.expanded = false;
        }
        if (!applet.expanded) {
            applet.expanded = true;
        }
        expanded = true;
    }

    function back() {
        if (fromQuickSettings) {
            closeActiveApplet();
        } else {
            expanded = false;
        }
    }

    function closeActiveApplet() {
        const a = activeApplet;
        activeApplet = null;
        fromQuickSettings = false;
        if (a) {
            a.expanded = false;
        }
    }

    onExpandedChanged: {
        if (!expanded) {
            closeActiveApplet();
        }
    }

    readonly property Connections activeAppletConnections: Connections {
        target: state.activeApplet
        function onExpandedChanged() {
            if (state.activeApplet && !state.activeApplet.expanded) {
                state.back();
            }
        }
    }
}
