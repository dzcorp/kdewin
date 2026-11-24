import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// Win11 Quick Settings tile: toggle button (+ optional details chevron) with the label underneath.
ColumnLayout {
    id: tile

    property string text
    property string subText
    property string iconName
    property bool checked: false
    property bool available: true
    property bool hasDetails: false

    signal toggled()
    signal detailsRequested()

    spacing: 6
    enabled: available
    opacity: available ? 1 : 0.5

    Rectangle {
        id: body
        Layout.fillWidth: true
        Layout.preferredHeight: lt.sizeQuicksettingsTileH
        radius: lt.radiusTile
        activeFocusOnTab: true
        color: {
            if (!tile.enabled) {
                return tile.checked ? lt.accentDisabled : lt.controlFillDisabled;
            }
            if (tile.checked) {
                return mainArea.pressed ? lt.accentFillPressed : mainArea.containsMouse ? lt.accentFillHover : lt.accentFill;
            }
            return mainArea.pressed ? lt.controlFillPressed : mainArea.containsMouse ? lt.controlFillHover : lt.controlFill;
        }
        border.width: 1
        border.color: tile.checked ? lt.controlStrokeOnAccent : lt.controlStroke
        Behavior on color { ColorAnimation { duration: lt.motionFasterMs } }

        Accessible.role: Accessible.CheckBox
        Accessible.name: tile.text
        Accessible.checkable: true
        Accessible.checked: tile.checked
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                tile.toggled();
                event.accepted = true;
            } else if (event.key === Qt.Key_Right && tile.hasDetails) {
                tile.detailsRequested();
                event.accepted = true;
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: lt.radiusTile + 3
            color: "transparent"
            border.width: 2
            border.color: lt.focusOuter
            visible: body.activeFocus
        }

        MouseArea {
            id: mainArea
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: tile.hasDetails ? divider.left : parent.right }
            hoverEnabled: true
            onClicked: tile.toggled()
        }

        Kirigami.Icon {
            roundToIconSize: false
            anchors.centerIn: mainArea
            width: lt.sizeQsIcon
            height: lt.sizeQsIcon
            source: tile.iconName
            color: tile.checked ? lt.textOnAccent : lt.textPrimary
            isMask: true
        }

        Rectangle {
            id: divider
            visible: tile.hasDetails
            width: 1
            anchors { right: detailsArea.left; top: parent.top; bottom: parent.bottom; topMargin: 1; bottomMargin: 1 }
            color: tile.checked ? lt.controlStrokeOnAccentSecondary : lt.controlStroke
        }

        MouseArea {
            id: detailsArea
            visible: tile.hasDetails
            width: visible ? 32 : 0
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            hoverEnabled: true
            onClicked: tile.detailsRequested()

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: lt.radiusTile - 1
                color: tile.checked ? lt.textOnAccent : lt.textPrimary
                opacity: detailsArea.pressed ? 0.12 : detailsArea.containsMouse ? 0.08 : 0
            }

            Kirigami.Icon {
                roundToIconSize: false
                anchors.centerIn: parent
                width: 12
                height: 12
                source: Application.layoutDirection === Qt.RightToLeft ? "arrow-left" : "arrow-right"
                color: tile.checked ? lt.textOnAccent : lt.textPrimary
                isMask: true
            }
        }
    }

    // Windows: label 9 px under the tile, 96 px rows.
    PlasmaComponents3.Label {
        Layout.fillWidth: true
        Layout.topMargin: 3
        Layout.bottomMargin: 11
        horizontalAlignment: Text.AlignHCenter
        text: tile.text
        elide: Text.ElideRight
        color: lt.textPrimary
        font.pixelSize: lt.typeCaption[0]
    }
}
