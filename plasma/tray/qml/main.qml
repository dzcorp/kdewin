pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

ContainmentItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    // Every icon (SNI pixmaps of any size, applet compact reps) is fitted into the same square box.
    readonly property int iconSize: Math.max(16, Math.min(32, Plasmoid.configuration.iconSize))
    readonly property int quickIconSize: Math.max(16, Math.min(32, Plasmoid.configuration.quickIconSize))
    readonly property int spacing: 16 + 4 * Math.max(0, Math.min(6, Plasmoid.configuration.iconSpacing))
    readonly property int cell: iconSize + spacing
    readonly property var quickSettingsIds: Plasmoid.configuration.quickSettingsItems
    readonly property var hiddenIds: Plasmoid.configuration.suppressedItems.concat(quickSettingsIds)
    readonly property string dragMime: "application/x-lumen-tray-item"
    readonly property Item panelContainment: {
        for (let item = root.parent; item; item = item.parent) {
            if (item.appletRequestsInhibitDnD !== undefined) {
                return item;
            }
        }
        return null;
    }

    Layout.minimumWidth: vertical ? root.iconSize : mainLayout.implicitWidth
    Layout.preferredWidth: Layout.minimumWidth
    Layout.minimumHeight: vertical ? mainLayout.implicitHeight : root.iconSize
    Layout.preferredHeight: Layout.minimumHeight

    Plasmoid.status: trayState.expanded || trayState.overflowOpen ? PlasmaCore.Types.RequiresAttentionStatus : PlasmaCore.Types.PassiveStatus

    LumenTokens { id: lt }
    TrayState { id: trayState }

    Binding {
        target: root.panelContainment
        property: "appletRequestsInhibitDnD"
        value: true
        when: root.panelContainment !== null && trayState.dragging
    }

    // Popup state lives in trayState only: a containment's own `expanded` always reads true in libplasma,
    // so mirroring it would reopen the popup right after it closes.
    Connections {
        target: trayState
        function onExpandedChanged() {
            if (!trayState.expanded) {
                activeModel.invalidateFilter();
                overflowModel.invalidateFilter();
            }
        }
    }
    Connections {
        target: Plasmoid
        function onActivated() {
            trayState.toggleQuickSettings();
        }
    }

    onHiddenIdsChanged: {
        activeModel.invalidateFilter();
        overflowModel.invalidateFilter();
        quickSettingsModel.invalidateFilter();
    }

    KItemModels.KSortFilterProxyModel {
        id: activeModel
        dynamicSortFilter: !trayState.expanded && !trayState.overflowOpen
        filterRoleName: "effectiveStatus"
        filterRowCallback: (row, parent) => {
            const idx = sourceModel.index(row, 0, parent);
            return sourceModel.data(idx, filterRole) === PlasmaCore.Types.ActiveStatus
                && root.hiddenIds.indexOf(sourceModel.data(idx, KItemModels.KRoleNames.role("itemId"))) < 0;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: overflowModel
        dynamicSortFilter: !trayState.overflowOpen
        filterRoleName: "effectiveStatus"
        filterRowCallback: (row, parent) => {
            const idx = sourceModel.index(row, 0, parent);
            return sourceModel.data(idx, filterRole) === PlasmaCore.Types.PassiveStatus
                && root.hiddenIds.indexOf(sourceModel.data(idx, KItemModels.KRoleNames.role("itemId"))) < 0;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: quickSettingsModel
        filterRowCallback: (row, parent) => {
            const idx = sourceModel.index(row, 0, parent);
            return sourceModel.data(idx, KItemModels.KRoleNames.role("itemType")) === "Plasmoid"
                && root.quickSettingsIds.indexOf(sourceModel.data(idx, KItemModels.KRoleNames.role("itemId"))) >= 0;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    // Any applet that expands itself (click, shortcut, notification) gets the shared popup.
    Instantiator {
        model: Plasmoid.systemTrayModel
        delegate: Connections {
            required property QtObject applet
            required property string itemId
            target: applet
            // Tray applets are always icons. With switch sizes, libplasma shows the full view inline whenever the
            // applet item is big enough (e.g. hidden applets nobody sized) and reports it as expanded, which would
            // reopen the popup in a loop.
            Component.onCompleted: {
                if (applet) {
                    applet.switchWidth = 0;
                    applet.switchHeight = 0;
                }
            }
            function onExpandedChanged(expanded: bool) {
                if (expanded && applet.fullRepresentationItem?.parent !== applet) {
                    trayState.openApplet(applet, root.quickSettingsIds.indexOf(itemId) >= 0);
                }
            }
        }
    }

    function appletItemFor(pluginId: string): Item {
        return Plasmoid.appletItem(pluginId);
    }

    // The full-size MouseArea of an applet's compact representation (to forward clicks / wheel).
    function appletMouseArea(applet: Item): MouseArea {
        const target = applet?.compactRepresentationItem ?? applet?.fullRepresentationItem ?? null;
        if (!target) {
            return null;
        }
        if (target instanceof MouseArea) {
            return target as MouseArea;
        }
        return target.children.find(c => c instanceof MouseArea && c.enabled
            && (c.anchors.fill === target || (c.x === 0 && c.y === 0 && c.width === target.width && c.height === target.height))) ?? null;
    }

    // Kirigami icons snap down to standard sizes (16/22/32...), so a 28 px box would still draw 22 px.
    // Hosted applets get exact-size icons; called on adoption and whenever the box is resized.
    // Applet icons snap to standard icon sizes until exactIcons() runs: run it before the first frame, then once more for
    // representations that build their icons a moment later.
    function fitIcons(item: Item) {
        exactIcons(item);
        Qt.callLater(() => root.exactIcons(item));
    }

    function exactIcons(item: Item) {
        if (!item) {
            return;
        }
        if (item.roundToIconSize === true) {
            item.roundToIconSize = false;
        }
        for (const child of item.children) {
            exactIcons(child);
        }
    }

    function dragId(event): string {
        return event.formats.indexOf(dragMime) >= 0 ? event.getDataAsString(dragMime) : "";
    }

    GridLayout {
        id: mainLayout
        anchors.fill: parent
        rowSpacing: 0
        columnSpacing: 0
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight

        OverflowChevron {
            id: chevron
            Layout.fillHeight: !root.vertical
            Layout.fillWidth: root.vertical
            Layout.preferredWidth: root.vertical ? -1 : root.cell
            Layout.preferredHeight: root.vertical ? root.cell : -1
            visible: overflowModel.count > 0 || trayState.overflowOpen || rowDrop.containsDrag
        }

        ListView {
            id: taskRow
            Layout.fillHeight: !root.vertical
            Layout.fillWidth: root.vertical
            Layout.preferredWidth: root.vertical ? -1 : contentWidth
            Layout.preferredHeight: root.vertical ? contentHeight : -1
            orientation: root.vertical ? ListView.Vertical : ListView.Horizontal
            interactive: false
            model: activeModel
            delegate: ItemLoader {
                width: root.vertical ? ListView.view.width : root.cell
                height: root.vertical ? root.cell : ListView.view.height
            }
            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: lt.motionNormalMs; easing.type: lt.easingDecel }
            }
            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: lt.motionFastMs }
            }

            DropArea {
                id: rowDrop
                anchors.fill: parent
                // Extra room only on the far end: the chevron before the row takes its own drops.
                anchors.rightMargin: root.vertical ? 0 : -root.cell / 2
                anchors.bottomMargin: root.vertical ? -root.cell / 2 : 0
                keys: [root.dragMime, "text/x-plasmoidservicename"]
                property int insertAt: -1

                function indexAt(drag): int {
                    const pos = root.vertical ? drag.y : drag.x;
                    return Math.max(0, Math.min(taskRow.count, Math.round(pos / root.cell)));
                }
                onEntered: drag => insertAt = indexAt(drag)
                onPositionChanged: drag => insertAt = indexAt(drag)
                onExited: insertAt = -1
                onDropped: drop => {
                    const id = root.dragId(drop);
                    if (id !== "") {
                        const at = indexAt(drop);
                        const before = at < taskRow.count ? (taskRow.itemAtIndex(at)?.itemId ?? "") : "";
                        Plasmoid.setItemVisibility(id, 1);
                        Plasmoid.moveItem(id, before);
                        drop.accept(Qt.MoveAction);
                    } else if (drop.formats.indexOf("text/x-plasmoidservicename") >= 0) {
                        const plugin = drop.getDataAsString("text/x-plasmoidservicename");
                        if (Plasmoid.isSystemTrayApplet(plugin) && Plasmoid.configuration.extraItems.indexOf(plugin) < 0) {
                            Plasmoid.configuration.extraItems = Plasmoid.configuration.extraItems.concat([plugin]);
                        }
                        drop.accept(Qt.CopyAction);
                    }
                    insertAt = -1;
                }
            }

            Rectangle {
                visible: rowDrop.containsDrag && rowDrop.insertAt >= 0
                color: lt.accentFill
                radius: 1
                width: root.vertical ? taskRow.width - 8 : 2
                height: root.vertical ? 2 : root.iconSize + 8
                x: root.vertical ? 4 : rowDrop.insertAt * root.cell - 1
                y: root.vertical ? rowDrop.insertAt * root.cell - 1 : (taskRow.height - height) / 2
            }
        }

        QuickSettingsButton {
            id: quickButton
            Layout.fillHeight: !root.vertical
            Layout.fillWidth: root.vertical
        }
    }

    Timer {
        id: expandedSync
        interval: 100
        onTriggered: trayState.expanded = dialog.visible
    }

    PlasmaCore.AppletPopup {
        id: dialog
        objectName: "popupWindow"
        visualParent: trayState.anchorItem ?? quickButton
        appletInterface: root
        popupDirection: switch (Plasmoid.location) {
            case PlasmaCore.Types.TopEdge: return Qt.BottomEdge;
            case PlasmaCore.Types.LeftEdge: return Qt.RightEdge;
            case PlasmaCore.Types.RightEdge: return Qt.LeftEdge;
            default: return Qt.TopEdge;
        }
        margin: lt.sizeFlyoutPad
        floating: Plasmoid.location === PlasmaCore.Types.Desktop
        removeBorderStrategy: PlasmaCore.AppletPopup.Never
        hideOnWindowDeactivate: !Plasmoid.configuration.pin
        backgroundHints: PlasmaCore.AppletPopup.StandardBackground
        visible: trayState.expanded
        onVisibleChanged: {
            if (!visible) {
                expandedSync.restart();
            } else {
                dialog.requestActivate();
                flyout.forceActiveFocus();
            }
        }
        mainItem: FlyoutContent {
            id: flyout
            Keys.onEscapePressed: trayState.expanded = false
        }
    }

    OverflowFlyout {
        id: overflow
        visualParent: chevron
    }
}
