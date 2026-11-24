import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.kicker as Kicker

KCM.SimpleKCM {
    id: page

    property var cfg_apps: []
    property alias cfg_expanding: expanding.checked

    Kicker.SimpleFavoritesModel {
        id: chosen
        favorites: page.cfg_apps
    }

    // Row 0 holds every installed app, alphabetical.
    Kicker.RootModel {
        id: root
        autoPopulate: false
        flat: true
        sorted: true
        showAllApps: true
        showTopLevelItems: true
        showSeparators: false
        showRecentApps: false
        showRecentDocs: false
        showPowerSession: false
        showFavoritesPlaceholder: false
        Component.onCompleted: refresh()
        onRefreshed: picker.model = modelForRow(0)
    }

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: expanding
            Kirigami.FormData.label: i18n("Size:")
            text: i18n("Fill free space")
        }

        Item { Kirigami.FormData.isSection: true }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Right-click menu:")
            spacing: 4

            Repeater {
                model: chosen
                delegate: RowLayout {
                    id: row
                    required property int index
                    required property string display
                    required property var decoration
                    spacing: 8
                    Kirigami.Icon {
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: implicitWidth
                        source: row.decoration
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.display
                    }
                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        text: i18n("Remove")
                        display: QQC2.AbstractButton.IconOnly
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        onClicked: page.cfg_apps = page.cfg_apps.filter((_, i) => i !== row.index)
                    }
                }
            }

            QQC2.ComboBox {
                id: picker
                textRole: "display"
                valueRole: "favoriteId"
                displayText: i18n("Add app…")
                onActivated: index => {
                    const id = String(valueAt(index) ?? "").replace(/^applications:/, "");
                    if (id && !page.cfg_apps.includes(id)) {
                        page.cfg_apps = page.cfg_apps.concat([id]);
                    }
                }
            }
        }
    }
}
