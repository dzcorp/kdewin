import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Applications")

    LumenHeader { title: i18n("Window material") }
    LumenCard {
        SettingSwitch {
            setting: "styleTranslucentWindows"
            text: i18n("Mica window backgrounds")
            description: i18n("Sidebars, toolbars and dialogs show the blurred wallpaper. Applies to newly opened windows.")
        }
        SettingSlider {
            setting: "styleWindowOpacity"
            enabled: kcm.settings.styleTranslucentWindows
            label: i18n("Opacity")
            from: 0
            to: 100
        }
        FormCard.FormTextFieldDelegate {
            enabled: kcm.settings.styleTranslucentWindows
            label: i18n("Keep these apps opaque")
            text: kcm.settings.styleOpaqueApps
            onEditingFinished: kcm.settings.styleOpaqueApps = text
        }
    }

    LumenHeader { title: i18n("Controls") }
    LumenCard {
        SettingSlider { setting: "styleControlRadius"; label: i18n("Corner radius"); from: 0; to: 12; suffix: " px" }
        SettingSwitch {
            setting: "styleTranslucentMenus"
            text: i18n("Acrylic menus and tooltips")
            description: i18n("Needs Lumen materials")
        }
        SettingSwitch {
            setting: "styleScrollbarExpanded"
            text: i18n("Always show full scroll bars")
            description: i18n("Off keeps them thin until hovered")
        }
    }

    LumenHeader { title: i18n("File Explorer") }
    LumenCard {
        SettingSwitch {
            setting: "filesCommandBar"
            text: i18n("Command bar")
            description: i18n("New, Cut, Copy, Paste, Sort and View buttons under the address bar. Off keeps a clean, minimal window.")
        }
        FormCard.FormComboBoxDelegate {
            text: i18n("Open File Explorer to")
            description: i18n("Home folder is the start location set in File Explorer's own settings")
            model: [i18n("This PC"), i18n("Previous session"), i18n("Recent files"), i18n("Home folder")]
            currentIndex: kcm.settings.filesOpenTo
            onActivated: index => kcm.settings.filesOpenTo = index
        }
    }

    LumenHeader { title: i18n("This PC") }
    LumenCard {
        SettingSwitch {
            setting: "filesThisPcFreeSpace"
            text: i18n("Show free space")
            description: i18n("A capacity bar and the free space under each drive")
        }
        SettingSwitch {
            setting: "filesThisPcFolders"
            text: i18n("Show folders")
            description: i18n("Desktop, Documents, Downloads, Music, Pictures and Videos above the drives")
        }
        SettingSwitch {
            setting: "filesThisPcNetwork"
            text: i18n("Show network locations")
        }
        SettingSwitch {
            setting: "filesThisPcHiddenDrives"
            text: i18n("Show hidden drives")
            description: i18n("Drives hidden from the navigation pane")
        }
    }

    LumenHeader { title: i18n("Motion") }
    LumenCard {
        SettingSwitch { setting: "styleAnimations"; text: i18n("Animate controls") }
        SettingSlider {
            setting: "styleAnimationDuration"
            enabled: kcm.settings.styleAnimations
            label: i18n("Duration")
            from: 25
            to: 400
        }
    }

    FormCard.FormSectionText {
        Layout.maximumWidth: 1000 - 2 * Kirigami.Units.smallSpacing
        Layout.leftMargin: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing
        text: i18n("Applies to apps using the Lumen application style. Changes show up live.")
    }
}
