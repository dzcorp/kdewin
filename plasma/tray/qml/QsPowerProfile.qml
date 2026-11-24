import QtQuick
import org.kde.plasma.private.batterymonitor

// Cycles power-saver -> balanced -> performance (Win11 "Power mode").
QuickTile {
    PowerProfilesControl { id: profiles; isSilent: true }

    readonly property var names: ({
        "power-saver": i18n("Power saver"),
        "balanced": i18n("Balanced"),
        "performance": i18n("Performance"),
    })

    // KDE icon names: the GNOME ones (power-profile-*) are missing from KDE themes.
    readonly property var icons: ({
        "power-saver": "battery-profile-powersave-symbolic",
        "balanced": "battery-profile-balanced-symbolic",
        "performance": "battery-profile-performance-symbolic",
    })

    text: names[profiles.activeProfile] ?? i18n("Power mode")
    iconName: icons[profiles.activeProfile] ?? icons.balanced
    available: profiles.isPowerProfileDaemonInstalled && profiles.profiles.length > 1
    checked: profiles.activeProfile !== "" && profiles.activeProfile !== "balanced"
    hasDetails: true
    onToggled: {
        const list = profiles.profiles;
        profiles.setProfile(list[(list.indexOf(profiles.activeProfile) + 1) % list.length]);
    }
    onDetailsRequested: qs.openPage("org.kde.plasma.battery")
}
