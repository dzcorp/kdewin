import QtQuick
import org.kde.plasma.networkmanagement as PlasmaNM

QuickTile {
    PlasmaNM.AvailableDevices { id: devices }
    PlasmaNM.Handler { id: handler }

    text: i18n("Airplane mode")
    iconName: "network-flightmode-on"
    available: devices.wirelessDeviceAvailable || devices.modemDeviceAvailable || devices.bluetoothDeviceAvailable
    checked: PlasmaNM.Configuration.airplaneModeEnabled
    onToggled: {
        const on = !checked;
        handler.enableAirplaneMode(on);
        PlasmaNM.Configuration.airplaneModeEnabled = on;
    }
}
