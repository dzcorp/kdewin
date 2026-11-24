import QtQuick
import org.kde.kcmutils as KCMUtils
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Windows")

    LumenHeader { title: i18n("Title bar") }
    LumenCard {
        SettingSwitch { setting: "decorationTranslucentTitlebar"; text: i18n("Translucent title bar") }
        SettingSlider {
            setting: "decorationTitlebarOpacity"
            enabled: kcm.settings.decorationTranslucentTitlebar
            label: i18n("Opacity")
            from: 0
            to: 100
        }
        SettingSlider { setting: "decorationTitlebarHeight"; label: i18n("Height"); from: 20; to: 64; suffix: " px" }
        FormCard.FormDelegateSeparator {}
        SettingSwitch { setting: "decorationCenterTitle"; text: i18n("Center the title") }
        SettingSwitch { setting: "decorationShowIcon"; text: i18n("Show the application icon") }
        FormCard.FormComboBoxDelegate {
            text: i18n("Buttons")
            model: [i18n("Minimize, maximize, close (right)"), i18n("Close only"), i18n("Close, minimize, maximize (left)")]
            currentIndex: kcm.settings.decorationButtonLayout
            onActivated: index => kcm.settings.decorationButtonLayout = index
        }
    }

    LumenHeader { title: i18n("Shape") }
    LumenCard {
        SettingSlider { setting: "decorationCornerRadius"; label: i18n("Corner radius"); from: 0; to: 24; suffix: " px" }
        FormCard.FormLinkDelegate {
            text: i18n("Window decoration settings")
            icon.name: "preferences-system-windows-actions"
            onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_kwindecoration")
        }
    }
}
