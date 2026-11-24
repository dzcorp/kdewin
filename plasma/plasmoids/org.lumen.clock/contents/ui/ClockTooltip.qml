pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.clock
import org.kde.plasma.components as PlasmaComponents3

// Clock tooltip: the date, today's holiday or event note, and the world clocks.
Item {
    implicitWidth: layout.implicitWidth + 24
    implicitHeight: layout.implicitHeight + 20

    component ZoneLabel: PlasmaComponents3.Label {
        id: zoneLabel
        required property string modelData
        required property int index
        readonly property alias zone: zoneClock
        Layout.row: index
        font.weight: modelData === root.zone ? Font.DemiBold : Font.Normal
        font.features: { "tnum": 1 }
        Clock {
            id: zoneClock
            timeZone: zoneLabel.modelData
        }
    }

    ColumnLayout {
        id: layout
        anchors.centerIn: parent
        spacing: 2

        PlasmaComponents3.Label {
            text: Qt.locale().toString(root.now, "dddd")
            font.pixelSize: lt.typeBodyStrong[0]
            font.weight: lt.typeBodyStrong[2]
        }
        PlasmaComponents3.Label {
            text: root.now.toLocaleDateString(Qt.locale(), Locale.LongFormat)
        }
        PlasmaComponents3.Label {
            readonly property string note: root.fullRepresentationItem?.todayAuxText ?? ""
            visible: note !== ""
            text: note
            color: lt.textSecondary
        }

        GridLayout {
            Layout.topMargin: 6
            visible: root.zones.length > 1
            columnSpacing: 16
            rowSpacing: 2

            Repeater {
                model: root.zones
                delegate: ZoneLabel {
                    Layout.column: 0
                    text: root.zoneName(zone) + ":"
                }
            }
            Repeater {
                model: root.zones
                delegate: ZoneLabel {
                    Layout.column: 1
                    text: root.formatZoneTime(zone.dateTime) + root.formatOffset(zone.dateTime)
                }
            }
        }
    }
}
