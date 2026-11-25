import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    id: page

    // Installed but not the active theme (fresh install, or switched away): one prominent way to turn it on.
    LumenCard {
        visible: !kcm.lumenActive
        FormCard.AbstractFormDelegate {
            background: null
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing
                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon {
                        source: "preferences-desktop-theme-global"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.large
                        Layout.preferredHeight: Kirigami.Units.iconSizes.large
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Kirigami.Heading {
                            level: 2
                            text: i18n("Lumen is not active")
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        QQC2.Label {
                            text: i18n("Theme, window decoration, effects, icons and File Explorer switch to the Windows 11 look.")
                            wrapMode: Text.Wrap
                            opacity: 0.8
                            Layout.fillWidth: true
                        }
                    }
                }
                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    QQC2.Button {
                        text: i18n("Apply Lumen")
                        icon.name: "dialog-ok-apply"
                        highlighted: true
                        enabled: kcm.toolAvailable && !kcm.busy
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 10
                        onClicked: kcm.run(["apply", kcm.settings.appearanceMode], false)
                    }
                    QQC2.Button {
                        text: i18n("Apply with taskbar")
                        icon.name: "preferences-desktop-panel"
                        enabled: kcm.toolAvailable && !kcm.busy
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                        onClicked: kcm.run(["apply", kcm.settings.appearanceMode, "--layout"], false)
                    }
                }
            }
        }
    }
    ToolLog {
        visible: kcm.log !== "" && !kcm.lumenActive
    }

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
