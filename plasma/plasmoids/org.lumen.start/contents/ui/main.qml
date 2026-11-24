pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.private.kicker as Kicker

PlasmoidItem {
    id: start

    readonly property string clientId: Plasmoid.configuration.favoritesClientId !== ""
        ? Plasmoid.configuration.favoritesClientId
        : "org.lumen.start.favorites.instance-" + Plasmoid.id
    readonly property var favorites: rootModel.favoritesModel
    property int appsRow: 1
    // Models beyond favorites load on first open (or once the session is idle), keeping shell start cheap.
    property bool warm: false
    readonly property var recentModel: recentLoader.object
    readonly property var mostUsedModel: mostUsedLoader.object

    preferredRepresentation: Plasmoid.formFactor === PlasmaCore.Types.Horizontal || Plasmoid.formFactor === PlasmaCore.Types.Vertical ? compactRepresentation : fullRepresentation
    hideOnWindowDeactivate: !Plasmoid.configuration.pin
    activationTogglesExpanded: true
    toolTipMainText: i18n("Start")
    toolTipSubText: ""
    Plasmoid.icon: Plasmoid.configuration.icon
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    LumenTokens { id: lt }

    function warmUp() {
        if (rootModel.count === 0) {
            rootModel.refresh();
        }
        warm = true;
    }

    onExpandedChanged: {
        if (start.expanded) {
            start.warmUp();
            start.placePopup();
        }
    }

    // Windows centres Start on the screen when the taskbar is centred (here: Start in the middle third of a horizontal
    // panel). The Lumen shell's CompactApplet anchors the popup to this item.
    property Item popupAnchor: null
    Item {
        id: screenCentre
        parent: start.Window.contentItem
        x: (Plasmoid.containment?.screenGeometry.x ?? 0) + (Plasmoid.containment?.screenGeometry.width ?? 0) / 2 - (start.Window.window?.x ?? 0)
        width: 1
        height: parent?.height ?? 0
    }
    function placePopup() {
        const button = start.compactRepresentationItem;
        const screen = Plasmoid.containment?.screenGeometry;
        if (!button || !screen || !start.Window.window || Plasmoid.formFactor !== PlasmaCore.Types.Horizontal) {
            popupAnchor = null;
            return;
        }
        const mid = start.Window.window.x + button.mapToItem(null, button.width / 2, 0).x;
        popupAnchor = Math.abs(mid - (screen.x + screen.width / 2)) < screen.width / 6 ? screenCentre : null;
    }

    Timer {
        interval: 6000
        running: !start.warm
        onTriggered: start.warmUp()
    }

    readonly property Kicker.RootModel rootModel: Kicker.RootModel {
        autoPopulate: false
        appletInterface: start
        appNameFormat: Plasmoid.configuration.appNameFormat
        flat: true
        sorted: true
        showSeparators: false
        showRootSeparator: false
        showTopLevelItems: true
        showAllApps: true
        showAllAppsCategorized: false
        showRecentApps: false
        showRecentDocs: false
        showPowerSession: false
        showFavoritesPlaceholder: true
        highlightNewlyInstalledApps: true

        Component.onCompleted: {
            favoritesModel.initForClient(start.clientId);
            if (!Plasmoid.configuration.favoritesPortedToKAstats) {
                if (favoritesModel.count < 1) {
                    favoritesModel.portOldFavorites(Plasmoid.configuration.favorites);
                }
                Plasmoid.configuration.favoritesPortedToKAstats = true;
            }
        }
        // modelForRow() creates no binding dependency; poke the row so views re-fetch.
        onRefreshed: {
            start.appsRow = 0;
            start.appsRow = 1;
        }
    }

    readonly property Kicker.RunnerModel runnerModel: Kicker.RunnerModel {
        appletInterface: start
        mergeResults: true
        favoritesModel: start.favorites
    }

    Instantiator {
        id: recentLoader
        active: start.warm
        delegate: Kicker.RecentUsageModel {
            favoritesModel: start.favorites
            shownItems: Plasmoid.configuration.recommendApps ? Kicker.RecentUsageModel.AppsAndDocs : Kicker.RecentUsageModel.OnlyDocs
            ordering: Kicker.RecentUsageModel.Recent
        }
    }

    Instantiator {
        id: mostUsedLoader
        active: start.warm && Plasmoid.configuration.showMostUsed
        delegate: Kicker.RecentUsageModel {
            favoritesModel: start.favorites
            shownItems: Kicker.RecentUsageModel.OnlyApps
            ordering: Kicker.RecentUsageModel.Popular
        }
    }

    // Opens the Settings app the same way the menu launches apps.
    readonly property Kicker.SimpleFavoritesModel settingsApp: Kicker.SimpleFavoritesModel {
        favorites: ["systemsettings.desktop"]
    }

    // Filtered views wrap Kicker models in proxies; actions run on the first model that has trigger(). Kicker's own
    // runner model is itself a proxy over KRunner's, which has no trigger().
    function resolve(model, row: int): var {
        while (typeof model?.trigger !== "function" && model?.mapToSource) {
            row = model.mapToSource(model.index(row, 0)).row;
            model = model.sourceModel;
        }
        return [model, row];
    }

    // Context-menu entries: pin/unpin to Start plus the item's own actions (incl. "Pin to Task Manager").
    function actionsFor(model, favoriteId): var {
        const actions = Array.from(model?.actionList ?? []).filter(a => a.type !== "title");
        if (favoriteId && favorites.enabled) {
            actions.unshift(favorites.isFavorite(favoriteId)
                ? { text: i18n("Unpin from Start"), icon: "window-unpin", actionId: "_lumen_unpin", actionArgument: favoriteId }
                : { text: i18n("Pin to Start"), icon: "window-pin", actionId: "_lumen_pin", actionArgument: favoriteId });
        }
        return actions;
    }

    function triggerAction(model, row: int, actionId: string, argument): bool {
        if (actionId === "_lumen_pin") {
            favorites.addFavorite(argument);
            return false;
        }
        if (actionId === "_lumen_unpin") {
            favorites.removeFavorite(argument);
            return false;
        }
        const [source, sourceRow] = resolve(model, row);
        const close = source.trigger(sourceRow, actionId, argument);
        if (close) {
            start.expanded = false;
        }
        return close;
    }

    function launch(model, row: int) {
        if (row < 0) {
            return;
        }
        const [source, sourceRow] = resolve(model, row);
        if (source.trigger(sourceRow, "", null)) {
            start.expanded = false;
        }
    }

    function openMenu(item: Item, model, row: int, pinnable: bool, x: real, y: real) {
        actionMenu.show(item, model, row, actionsFor(item.model, pinnable ? item.model.favoriteId : ""), x, y);
    }

    ActionMenu { id: actionMenu }

    // Shared drag source: a delegate may be destroyed mid-drag (e.g. when the model reorders).
    readonly property Item dragSource: Item {
        property Item sourceItem
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.LinkAction | Qt.MoveAction
    }

    function drag(active: bool, item: Item, icon: Item) {
        if (!active) {
            dragSource.Drag.active = false;
            dragSource.Drag.imageSource = "";
            dragSource.sourceItem = null;
            return;
        }
        icon.grabToImage(result => {
            if (!item.dragging) {
                return;
            }
            dragSource.sourceItem = item;
            dragSource.Drag.imageSource = result.url;
            dragSource.Drag.mimeData = { "text/uri-list": [item.model.url.toString()] };
            dragSource.Drag.active = true;
        });
    }

    compactRepresentation: MouseArea {
        id: button

        // Icon sizes count the icon's padding: the default glyph fills 75 % of it (24 px in a 32 px icon, as Windows).
        Layout.minimumWidth: Math.max(lt.sizeTaskbarButton, Plasmoid.configuration.iconSize + 4)
        Layout.preferredWidth: Layout.minimumWidth
        Layout.minimumHeight: lt.sizeTaskbarButton
        hoverEnabled: true
        activeFocusOnTab: true
        Accessible.name: i18n("Start")
        Accessible.role: Accessible.Button
        onClicked: start.expanded = !start.expanded
        onContainsMouseChanged: if (containsMouse) start.warmUp()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                start.expanded = !start.expanded;
                event.accepted = true;
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 4, lt.sizeTaskbarPlate)
            height: Math.min(parent.height, lt.sizeTaskbarPlate)
            radius: lt.radiusTaskbarButton
            color: button.pressed ? lt.controlFillPressed : lt.controlFillHover
            border.color: lt.controlStroke
            opacity: button.containsMouse || start.expanded ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
        }

        Kirigami.Icon {
            roundToIconSize: false
            anchors.centerIn: parent
            width: Math.min(Plasmoid.configuration.iconSize, button.height - 2)
            height: width
            source: Plasmoid.icon
            active: button.containsMouse
            scale: button.pressed ? 0.85 : 1
            Behavior on scale { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }
        }
    }

    fullRepresentation: StartMenu {}

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("Edit Applications…")
            icon.name: "kmenuedit"
            visible: Plasmoid.immutability !== PlasmaCore.Types.SystemImmutable
            onTriggered: processRunner.runMenuEditor()
        }
    ]

    Kicker.ProcessRunner { id: processRunner }
}
