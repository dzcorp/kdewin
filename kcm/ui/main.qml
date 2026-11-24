import QtQuick
import org.kde.kcmutils as KCMUtils
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    id: page

    readonly property var modes: [
        { id: "dark", label: i18n("Dark"), help: i18n("Mica-dark surfaces everywhere") },
        { id: "light", label: i18n("Light"), help: i18n("Bright surfaces with soft shadows") },
        { id: "auto", label: i18n("Automatic"), help: i18n("Switch between light and dark with the time of day") },
    ]

    LumenHeader { title: i18n("Mode") }
    LumenCard {
        Repeater {
            model: page.modes
            delegate: FormCard.FormRadioDelegate {
                required property var modelData
                text: modelData.label
                description: modelData.help
                checked: kcm.settings.appearanceMode === modelData.id
                onToggled: if (checked) kcm.settings.appearanceMode = modelData.id
            }
        }
    }

    LumenHeader { title: i18n("Text size") }
    LumenCard {
        SettingSlider { setting: "fontsTextSize"; label: i18n("Text size"); from: 100; to: 225; stepSize: 1 }
    }

    LumenHeader { title: i18n("Customize") }
    LumenCard {
        Repeater {
            model: [
                { page: "Materials.qml", icon: "preferences-desktop-effects", label: i18n("Materials"), help: i18n("Mica and acrylic blur, tint and noise") },
                { page: "Windows.qml", icon: "preferences-system-windows", label: i18n("Windows"), help: i18n("Title bars, corners and buttons") },
                { page: "Applications.qml", icon: "preferences-desktop-theme-applications", label: i18n("Applications"), help: i18n("Controls, menus, scroll bars and animation speed") },
                { page: "Animations.qml", icon: "preferences-desktop-animations", label: i18n("Animations"), help: i18n("Open, close and minimize motion") },
                { page: "Taskbar.qml", icon: "preferences-desktop-plasma", label: i18n("Taskbar"), help: i18n("Alignment, size, Start, tray and clock") },
                { page: "LockLogin.qml", icon: "system-lock-screen", label: i18n("Lock and login"), help: i18n("Lock screen, login screen and logout") },
                { page: "Boot.qml", icon: "preferences-system-startup", label: i18n("Boot"), help: i18n("Boot menu and boot splash") },
                { page: "Maintenance.qml", icon: "tools-wizard", label: i18n("Maintenance"), help: i18n("Repair, update and diagnostics") },
            ]
            delegate: FormCard.FormButtonDelegate {
                required property var modelData
                icon.name: modelData.icon
                text: modelData.label
                description: modelData.help
                onClicked: kcm.push(modelData.page)
            }
        }
    }

    LumenHeader { title: i18n("Related") }
    LumenCard {
        Repeater {
            model: [
                { kcm: "kcm_colors", icon: "preferences-desktop-color", label: i18n("Accent color") },
                { kcm: "kcm_wallpaper", icon: "preferences-desktop-wallpaper", label: i18n("Wallpaper") },
                { kcm: "kcm_fonts", icon: "preferences-desktop-font", label: i18n("Fonts") },
                { kcm: "kcm_icons", icon: "preferences-desktop-icons", label: i18n("Icons") },
            ]
            delegate: FormCard.FormLinkDelegate {
                required property var modelData
                icon.name: modelData.icon
                text: modelData.label
                onClicked: KCMUtils.KCMLauncher.openSystemSettings(modelData.kcm)
            }
        }
    }
    ToolLog {}
}
