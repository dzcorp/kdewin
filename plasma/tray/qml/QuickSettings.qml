pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

// Win11 Quick Settings. Each tile / slider lives in its own file loaded on demand,
// so a missing backend module (e.g. no bluedevil) only hides that control.
FocusScope {
    id: qs

    readonly property int pad: 12 // plus the 12 px dialog margin: 24, as in Windows
    readonly property var tileSources: ({
        "wifi": "QsWifi.qml",
        "bluetooth": "QsBluetooth.qml",
        "airplane": "QsAirplane.qml",
        "powerprofile": "QsPowerProfile.qml",
        "nightlight": "QsNightLight.qml",
        "dnd": "QsDnd.qml",
        "darkmode": "QsDarkMode.qml",
    })

    implicitHeight: body.implicitHeight + qs.pad

    function openPage(pluginId: string) {
        const applet = Plasmoid.appletItem(pluginId);
        if (applet) {
            trayState.openApplet(applet, true);
        }
    }

    ColumnLayout {
        id: body
        anchors { fill: parent; margins: qs.pad; bottomMargin: 0 }
        spacing: 16

        Loader {
            Layout.fillWidth: true
            source: "QsMedia.qml"
            visible: status === Loader.Ready && item.available
        }

        GridLayout {
            Layout.fillWidth: true
            columns: lt.sizeQuicksettingsCols
            columnSpacing: 12
            rowSpacing: 12
            uniformCellWidths: true

            Repeater {
                model: Plasmoid.configuration.quickSettingsTiles
                delegate: Loader {
                    required property string modelData
                    Layout.fillWidth: true
                    source: qs.tileSources[modelData] ?? ""
                    visible: status === Loader.Ready && item.available
                    focus: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: -qs.pad - lt.sizeFlyoutPad
            Layout.rightMargin: -qs.pad - lt.sizeFlyoutPad
            Layout.topMargin: -6
            Layout.preferredHeight: 1
            color: lt.divider
        }

        Loader {
            Layout.fillWidth: true
            Layout.topMargin: 6
            source: "QsBrightness.qml"
            visible: status === Loader.Ready && item.available
        }

        Loader {
            Layout.fillWidth: true
            source: "QsVolume.qml"
            visible: status === Loader.Ready && item.available
        }

        Item { Layout.fillHeight: true }

        // Footer band like Windows: battery on the left, Settings on the right.
        Item {
            Layout.fillWidth: true
            Layout.topMargin: 3
            Layout.preferredHeight: 48 - lt.sizeFlyoutPad

            Rectangle {
                anchors { fill: parent; leftMargin: -qs.pad - lt.sizeFlyoutPad; rightMargin: -qs.pad - lt.sizeFlyoutPad; bottomMargin: -lt.sizeFlyoutPad }
                bottomLeftRadius: lt.radiusOverlay
                bottomRightRadius: lt.radiusOverlay
                color: lt.dark ? "#14000000" : "#05000000"
                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: lt.divider
                }
            }

            RowLayout {
                anchors { fill: parent; topMargin: 1; bottomMargin: -lt.sizeFlyoutPad; rightMargin: -8 }
                Loader {
                    source: "QsBattery.qml"
                    visible: status === Loader.Ready && item.available
                }
                Item { Layout.fillWidth: true }
                PlasmaComponents3.ToolButton {
                    icon.name: "settings-configure-symbolic"
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    text: i18n("Settings")
                    PlasmaComponents3.ToolTip.text: text
                    PlasmaComponents3.ToolTip.visible: hovered
                    onClicked: {
                        trayState.expanded = false;
                        KCMUtils.KCMLauncher.openSystemSettings("");
                    }
                }
            }
        }
    }
}
