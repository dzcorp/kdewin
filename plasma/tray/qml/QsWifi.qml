import QtQuick
import org.kde.plasma.networkmanagement as PlasmaNM

QuickTile {
    PlasmaNM.EnabledConnections { id: connections }
    PlasmaNM.AvailableDevices { id: devices }
    PlasmaNM.Handler { id: handler }

    readonly property bool airplane: PlasmaNM.Configuration.airplaneModeEnabled

    text: i18n("Wi-Fi")
    iconName: checked ? "network-wireless-symbolic" : "network-wireless-disconnected-symbolic"
    available: devices.wirelessDeviceAvailable && connections.wirelessHwEnabled
    checked: connections.wirelessEnabled && !airplane
    hasDetails: true
    onToggled: handler.enableWireless(!connections.wirelessEnabled)
    onDetailsRequested: qs.openPage("org.kde.plasma.networkmanagement")
}
