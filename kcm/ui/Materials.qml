import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Materials")

    LumenCard {
        SettingSwitch {
            setting: "materialsEnabled"
            text: i18n("Lumen materials")
            description: i18n("Replaces the stock blur with Mica and acrylic")
        }
        FormCard.FormDelegateSeparator {}
        SettingSwitch {
            setting: "materialsMicaForPanels"
            enabled: kcm.settings.materialsEnabled
            text: i18n("Mica for the taskbar")
            description: i18n("Off uses acrylic, which shows windows behind the taskbar")
        }
        FormCard.FormDelegateSeparator {}
        SettingSlider {
            setting: "materialsWallpaperTone"
            enabled: kcm.settings.materialsEnabled
            label: i18n("Wallpaper tone")
            description: i18n("Taskbar, Start, menus and windows take on the wallpaper's color and follow it smoothly when it changes")
            from: 0
            to: 100
        }
    }

    LumenHeader { title: i18n("Acrylic (menus, flyouts, Start)") }
    LumenCard {
        enabled: kcm.settings.materialsEnabled
        SettingSlider { setting: "materialsAcrylicStrength"; label: i18n("Blur"); from: 1; to: 15; suffix: "" }
        SettingSlider { setting: "materialsAcrylicSaturation"; label: i18n("Saturation"); from: 100; to: 300 }
        SettingSlider { setting: "materialsAcrylicNoise"; label: i18n("Noise"); from: 0; to: 14; suffix: "" }
    }

    LumenHeader { title: i18n("Mica (windows, taskbar)") }
    LumenCard {
        enabled: kcm.settings.materialsEnabled
        SettingSlider { setting: "materialsMicaStrength"; label: i18n("Blur"); from: 1; to: 15; suffix: "" }
        SettingSlider { setting: "materialsMicaSaturation"; label: i18n("Saturation"); from: 100; to: 250 }
    }
}
