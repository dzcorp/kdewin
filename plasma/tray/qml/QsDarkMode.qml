import QtQuick
import org.kde.plasma.private.brightnesscontrolplugin

// Switches between the day/night global themes (Lumen light / dark).
QuickTile {
    text: i18n("Dark mode")
    iconName: "weather-clear-night-symbolic"
    available: DarkModeControl.otherTheme !== ""
    checked: DarkModeControl.darkMode
    onToggled: DarkModeControl.darkMode = !DarkModeControl.darkMode
}
