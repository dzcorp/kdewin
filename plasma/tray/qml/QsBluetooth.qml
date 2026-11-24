import QtQuick
import org.kde.bluezqt as BluezQt

QuickTile {
    text: i18n("Bluetooth")
    iconName: checked ? "network-bluetooth-activated-symbolic" : "network-bluetooth-inactive-symbolic"
    available: BluezQt.Manager.bluetoothBlocked || BluezQt.Manager.adapters.length > 0
    checked: BluezQt.Manager.bluetoothOperational
    hasDetails: true
    onToggled: {
        const on = !checked;
        BluezQt.Manager.bluetoothBlocked = !on;
        BluezQt.Manager.adapters.forEach(adapter => adapter.powered = on);
    }
    onDetailsRequested: qs.openPage("org.kde.plasma.bluetooth")
}
