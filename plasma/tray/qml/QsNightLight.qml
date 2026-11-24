import QtQuick
import org.kde.kcmutils as KCMUtils
import org.kde.plasma.private.brightnesscontrolplugin
import org.kde.plasma.workspace.dbus as DBus

QuickTile {
    DBus.Properties {
        id: nightLight
        busType: DBus.BusType.Session
        service: "org.kde.KWin.NightLight"
        path: "/org/kde/KWin/NightLight"
        iface: "org.kde.KWin.NightLight"
        readonly property bool available: Boolean(properties.available)
        readonly property bool enabled: Boolean(properties.enabled)
        readonly property bool inhibited: Boolean(properties.inhibited)
    }

    text: i18n("Night light")
    iconName: checked ? "redshift-status-on" : "redshift-status-off"
    available: nightLight.available
    checked: nightLight.enabled && !nightLight.inhibited
    hasDetails: true
    onToggled: {
        if (!nightLight.enabled) {
            KCMUtils.KCMLauncher.openSystemSettings("kcm_nightlight");
            trayState.expanded = false;
        } else if (!nightLight.inhibited || NightLightInhibitor.inhibited) {
            NightLightInhibitor.toggleInhibition();
        }
    }
    onDetailsRequested: qs.openPage("org.kde.plasma.brightness")
}
