import QtQuick
import org.kde.kcmutils as KCMUtils
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Lock and login")

    LumenHeader { title: i18n("Lock screen") }
    LumenCard {
        SettingSwitch { setting: "lockscreenLumenWallpaper"; text: i18n("Lumen wallpaper") }
        SettingSwitch { setting: "lockscreenAlwaysShowClock"; text: i18n("Always show the clock") }
        SettingSwitch { setting: "lockscreenMediaControls"; text: i18n("Media controls") }
        FormCard.FormLinkDelegate {
            text: i18n("Screen locking")
            icon.name: "system-lock-screen"
            onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_screenlocker")
        }
    }

    LumenHeader { title: i18n("Login screen") }
    LumenCard {
        ToolAction { text: i18n("Use Lumen on the login screen"); args: ["login", "on"]; asRoot: true }
        ToolAction { text: i18n("Restore the stock login screen"); args: ["login", "off"]; asRoot: true }
        FormCard.FormLinkDelegate {
            text: i18n("Login screen settings")
            icon.name: "preferences-system-login"
            onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_plasmalogin")
        }
    }

    LumenHeader { title: i18n("Lock and logout screens") }
    LumenCard {
        ToolAction {
            text: i18n("Refresh the Lumen lock and logout screens")
            description: i18n("Runs automatically after Plasma updates")
            args: ["refresh-shell"]
            asRoot: true
        }
    }

    ToolLog {}
}
