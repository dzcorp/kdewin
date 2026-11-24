import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.iconthemes as KIconThemes
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: page

    property string cfg_icon
    property alias cfg_appNameFormat: nameFormat.currentIndex
    property bool cfg_singlePage
    property alias cfg_startLayout: balance.currentIndex
    property alias cfg_showRecommended: recommended.checked
    property alias cfg_recommendApps: recommendApps.checked
    property alias cfg_showRecentlyAdded: recentlyAdded.checked
    property alias cfg_showMostUsed: mostUsed.checked
    property alias cfg_showCategories: categories.checked
    property int cfg_appsView
    property var cfg_footerFolders: []

    readonly property var folders: [
        ["settings", i18n("Settings")],
        ["files", i18n("File Manager")],
        ["documents", i18n("Documents")],
        ["downloads", i18n("Downloads")],
        ["music", i18n("Music")],
        ["pictures", i18n("Pictures")],
        ["videos", i18n("Videos")],
        ["network", i18n("Network")],
    ]

    Kirigami.FormLayout {
        QQC2.Button {
            Kirigami.FormData.label: i18n("Icon:")
            icon.name: page.cfg_icon
            text: i18n("Choose…")
            onClicked: picker.open()
            KIconThemes.IconDialog {
                id: picker
                onIconNameChanged: name => page.cfg_icon = name || "start-here-kde-symbolic"
            }
        }
        QQC2.ComboBox {
            id: nameFormat
            Kirigami.FormData.label: i18n("App names:")
            model: [i18n("Name only"), i18n("Generic name only"), i18n("Name (generic name)"), i18n("Generic name (name)")]
        }

        Item { Kirigami.FormData.isSection: true }

        QQC2.RadioButton {
            Kirigami.FormData.label: i18n("Layout:")
            text: i18n("Pinned and All apps on separate pages")
            checked: !page.cfg_singlePage
            onToggled: page.cfg_singlePage = false
        }
        QQC2.RadioButton {
            text: i18n("Everything on one scrolling page")
            checked: page.cfg_singlePage
            onToggled: page.cfg_singlePage = true
        }
        QQC2.ComboBox {
            id: balance
            Kirigami.FormData.label: i18n("Show:")
            model: [i18n("Default"), i18n("More pins"), i18n("More recommendations")]
        }

        Item { Kirigami.FormData.isSection: true }

        QQC2.CheckBox {
            id: recommended
            Kirigami.FormData.label: i18n("Recommended:")
            text: i18n("Show recent files")
        }
        QQC2.CheckBox {
            id: recommendApps
            enabled: recommended.checked
            text: i18n("Include recently used apps")
        }

        Item { Kirigami.FormData.isSection: true }

        QQC2.CheckBox {
            id: recentlyAdded
            Kirigami.FormData.label: i18n("All apps:")
            text: i18n("Show recently added apps")
        }
        QQC2.CheckBox {
            id: mostUsed
            text: i18n("Show most used apps")
        }
        QQC2.CheckBox {
            id: categories
            text: i18n("Enable category view (application menu groups)")
            onToggled: if (!checked && page.cfg_appsView === 2) page.cfg_appsView = 0
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("View:")
            model: categories.checked ? [i18n("List"), i18n("Grid"), i18n("Category")] : [i18n("List"), i18n("Grid")]
            currentIndex: Math.min(page.cfg_appsView, count - 1)
            onActivated: index => page.cfg_appsView = index
        }

        Item { Kirigami.FormData.isSection: true }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Next to Power:")
            spacing: 0
            Repeater {
                model: page.folders
                delegate: QQC2.CheckBox {
                    required property var modelData
                    text: modelData[1]
                    checked: page.cfg_footerFolders.includes(modelData[0])
                    onToggled: {
                        const on = new Set(page.cfg_footerFolders);
                        if (checked) {
                            on.add(modelData[0]);
                        } else {
                            on.delete(modelData[0]);
                        }
                        page.cfg_footerFolders = page.folders.map(f => f[0]).filter(id => on.has(id));
                    }
                }
            }
        }
    }
}
