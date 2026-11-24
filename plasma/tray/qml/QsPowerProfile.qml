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

    text: names[profiles.activeProfile] ?? i18n("Power mode")
    iconName: "power-profile-" + (profiles.activeProfile || "balanced") + "-symbolic"
    available: profiles.isPowerProfileDaemonInstalled && profiles.profiles.length > 1
    checked: profiles.activeProfile !== "" && profiles.activeProfile !== "balanced"
    hasDetails: true
    onToggled: {
        const list = profiles.profiles;
        profiles.setProfile(list[(list.indexOf(profiles.activeProfile) + 1) % list.length]);
    }
    onDetailsRequested: qs.openPage("org.kde.plasma.battery")
}
