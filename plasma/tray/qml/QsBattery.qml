import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.private.battery

RowLayout {
    BatteryControlModel { id: battery }

    readonly property bool available: battery.hasInternalBatteries
    readonly property bool charging: battery.state === BatteryControlModel.Charging

    spacing: 6

    Kirigami.Icon {
        roundToIconSize: false
        Layout.preferredWidth: lt.sizeQsIcon
        Layout.preferredHeight: lt.sizeQsIcon
        source: "battery-" + String(Math.round(battery.percent / 10) * 10).padStart(3, "0") + (parent.charging ? "-charging" : "") + "-symbolic"
    }
    PlasmaComponents3.Label {
        text: i18nc("battery percentage", "%1%", battery.percent)
        font.pixelSize: lt.typeCaption[0]
    }

    TapHandler {
        onTapped: qs.openPage("org.kde.plasma.battery")
    }
}
