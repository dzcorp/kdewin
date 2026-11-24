import QtQuick
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Animations")

    LumenCard {
        SettingSwitch { setting: "animationsWindowOpen"; text: i18n("Open and close"); description: i18n("Fade and zoom windows in and out") }
        SettingSlider {
            setting: "animationsOpenScale"
            enabled: kcm.settings.animationsWindowOpen
            label: i18n("Start size")
            from: 50
            to: 100
        }
        FormCard.FormDelegateSeparator {}
        SettingSwitch { setting: "animationsMinimize"; text: i18n("Minimize"); description: i18n("Windows fly to their taskbar button") }
        FormCard.FormDelegateSeparator {}
        SettingSlider { setting: "animationsWindowDuration"; label: i18n("Duration"); from: 25; to: 400 }
    }

    LumenCard {
        Layout.topMargin: 12
        FormCard.FormLinkDelegate {
            text: i18n("All desktop effects")
            icon.name: "preferences-desktop-effects"
            onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_kwin_effects")
        }
    }
}
