pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

// Changes apply immediately (same storage the drag-and-drop in the panel uses).
KCM.ScrollViewKCM {
    id: page

    readonly property var quickSettingsIds: Plasmoid.configuration.quickSettingsItems
    readonly property var allTiles: ["wifi", "bluetooth", "airplane", "powerprofile", "nightlight", "dnd", "darkmode"]
    readonly property var tileNames: ({
        "wifi": i18n("Wi-Fi"),
        "bluetooth": i18n("Bluetooth"),
        "airplane": i18n("Airplane mode"),
        "powerprofile": i18n("Power mode"),
        "nightlight": i18n("Night light"),
        "dnd": i18n("Do not disturb"),
        "darkmode": i18n("Dark mode"),
    })

    function toggleIn(key: string, id: string, on: bool) {
        const list = Plasmoid.configuration[key].filter(x => x !== id);
        if (on) {
            list.push(id);
        }
        Plasmoid.configuration[key] = list;
    }

    function isDisabled(model): bool {
        return model.itemType === "Plasmoid" ? !model.hasApplet && Plasmoid.configuration.extraItems.indexOf(model.itemId) < 0
                                             : Plasmoid.configuration.disabledStatusNotifiers.indexOf(model.itemId) >= 0;
    }

    function setMode(model, mode: int) {
        if (mode === 3) {
            if (model.itemType === "Plasmoid") {
                toggleIn("extraItems", model.itemId, false);
            } else {
                toggleIn("disabledStatusNotifiers", model.itemId, true);
            }
            Plasmoid.setItemVisibility(model.itemId, 0);
            return;
        }
        if (model.itemType === "Plasmoid") {
            toggleIn("extraItems", model.itemId, true);
        } else {
            toggleIn("disabledStatusNotifiers", model.itemId, false);
        }
        Plasmoid.setItemVisibility(model.itemId, mode);
    }

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            text: i18n("Tip: drag icons on the taskbar or within the Quick Settings button to reorder them. Drag other tray icons between the taskbar and the hidden icons flyout to pin or hide them.")
        }
        Kirigami.Heading {
            level: 4
            text: i18n("Quick Settings tiles")
        }
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: page.allTiles
                delegate: QQC2.CheckBox {
                    required property string modelData
                    text: page.tileNames[modelData]
                    checked: Plasmoid.configuration.quickSettingsTiles.indexOf(modelData) >= 0
                    onToggled: page.toggleIn("quickSettingsTiles", modelData, checked)
                }
            }
        }
        Kirigami.Heading {
            level: 4
            text: i18n("Icons")
        }
    }

    view: ListView {
        model: Plasmoid.configSystemTrayModel
        reuseItems: true
        delegate: QQC2.ItemDelegate {
            id: row
            required property var model
            required property int index
            width: ListView.view.width
            hoverEnabled: false
            down: false

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Icon {
                    roundToIconSize: false
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    source: row.model.decoration
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.model.displayText || row.model.itemId
                    elide: Text.ElideRight
                }
                QQC2.CheckBox {
                    visible: row.model.itemType === "Plasmoid"
                    text: i18n("In Quick Settings")
                    checked: page.quickSettingsIds.indexOf(row.model.itemId) >= 0
                    onToggled: page.toggleIn("quickSettingsItems", row.model.itemId, checked)
                }
                QQC2.ComboBox {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                    enabled: page.quickSettingsIds.indexOf(row.model.itemId) < 0
                    model: [i18n("Shown when relevant"), i18n("Always shown"), i18n("Always hidden"), i18n("Disabled")]
                    currentIndex: page.isDisabled(row.model) ? 3 : Plasmoid.itemVisibility(row.model.itemId)
                    onActivated: index => page.setMode(row.model, index)
                }
            }
        }
    }
}
