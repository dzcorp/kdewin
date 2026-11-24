pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.private.kicker as Kicker

// Taskbar gap: two of these center Start and the apps. Right-click lists chosen apps
// (Task Manager by default) and the taskbar settings, like the Windows taskbar.
PlasmoidItem {
    id: root

    readonly property bool horizontal: Plasmoid.formFactor !== PlasmaCore.Types.Vertical
    readonly property bool expanding: Plasmoid.configuration.expanding
    readonly property bool editing: Plasmoid.containment?.corona?.editMode ?? false
    readonly property real size: expanding ? optimalSize : Math.max(0, Plasmoid.configuration.length)

    readonly property GridLayout panelLayout: {
        for (let candidate = root.parent; candidate; candidate = candidate.parent) {
            if (candidate instanceof GridLayout) {
                return candidate;
            }
        }
        return null;
    }

    // Same rule as Plasma's spacer: expanding spacers split the panel so the groups between them
    // sit centered on the screen, not just in the space left over.
    readonly property real optimalSize: {
        if (!panelLayout) {
            return 0;
        }
        let spacers = 0;
        let mine = -1;
        const hints = [0];
        for (const child of panelLayout.children) {
            if (!child.visible) {
                continue;
            }
            const plasmoid = child.applet?.plasmoid;
            if ((plasmoid?.pluginName === "org.lumen.spacer" || plasmoid?.pluginName === "org.kde.plasma.panelspacer") && plasmoid.configuration.expanding) {
                if (plasmoid === Plasmoid) {
                    mine = spacers;
                }
                hints.push(0);
                spacers += 1;
            } else if (horizontal) {
                hints[hints.length - 1] += Math.min(child.Layout.maximumWidth, Math.max(child.Layout.minimumWidth, child.Layout.preferredWidth)) + panelLayout.rowSpacing;
            } else {
                hints[hints.length - 1] += Math.min(child.Layout.maximumHeight, Math.max(child.Layout.minimumHeight, child.Layout.preferredHeight)) + panelLayout.columnSpacing;
            }
        }
        if (mine < 0) {
            return 0;
        }
        hints[0] *= 2;
        hints[hints.length - 1] *= 2;
        // The panel layout's own size notifies on every resize, so the split follows live.
        const total = horizontal ? panelLayout.width : panelLayout.height;
        return Math.max(0, total / spacers - hints[mine] / 2 - hints[mine + 1] / 2);
    }

    // Compact only: an inline full view counts as an open popup and the panel underlines it.
    preferredRepresentation: compactRepresentation
    activationTogglesExpanded: false
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    Layout.fillWidth: horizontal && expanding
    Layout.fillHeight: !horizontal && expanding
    Layout.minimumWidth: horizontal ? (editing ? 32 : 1) : 0
    Layout.minimumHeight: horizontal ? 0 : (editing ? 32 : 1)
    Layout.preferredWidth: horizontal ? size : 0
    Layout.preferredHeight: horizontal ? 0 : size

    LumenTokens { id: lt }

    // Shows the gap while the panel is being edited.
    compactRepresentation: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            visible: root.editing
            radius: lt.radiusControl
            color: "transparent"
            border.width: 1
            border.color: lt.controlStrongFill
            opacity: 0.5
        }
    }

    // All spacers in the panel share one app list: loading merges them, an edit in one reaches the others.
    readonly property var chosenApps: Plasmoid.configuration.apps
    onChosenAppsChanged: share(chosenApps)
    Component.onCompleted: merge()

    Connections {
        target: Plasmoid.containment
        function onAppletsChanged() {
            root.merge();
        }
    }

    function spacers() {
        return (Plasmoid.containment?.applets ?? []).filter(a => a.pluginName === "org.lumen.spacer");
    }

    function share(list) {
        const key = JSON.stringify(list);
        for (const a of spacers()) {
            if (JSON.stringify(a.configuration.apps) !== key) {
                a.configuration.apps = list;
            }
        }
    }

    function merge() {
        const all = [];
        for (const a of spacers()) {
            for (const id of a.configuration.apps) {
                if (!all.includes(id)) {
                    all.push(id);
                }
            }
        }
        share(all);
    }

    Kicker.SimpleFavoritesModel {
        id: apps
        favorites: Plasmoid.configuration.apps
    }

    Instantiator {
        id: appActions
        // Kicker resets the model on every favorites write (it rewrites ids), which recreates
        // all actions while count stays the same; count alone leaves the menu with dead actions.
        property int revision: 0
        onObjectAdded: ++revision
        onObjectRemoved: ++revision
        model: apps
        delegate: PlasmaCore.Action {
            required property int index
            required property string display
            required property string decoration
            text: display
            icon.name: decoration
            onTriggered: apps.trigger(index, "", null)
        }
    }

    PlasmaCore.Action {
        id: separator
        isSeparator: true
    }

    PlasmaCore.Action {
        id: settings
        text: i18n("Taskbar settings")
        icon.name: "configure"
        onTriggered: KCMUtils.KCMLauncher.openSystemSettings("kcm_lumen")
    }

    Plasmoid.contextualActions: {
        appActions.revision;
        const list = [];
        for (let i = 0; i < appActions.count; ++i) {
            list.push(appActions.objectAt(i));
        }
        if (list.length > 0) {
            list.push(separator);
        }
        list.push(settings);
        return list;
    }
}
