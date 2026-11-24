import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// Borderless text-and-chevron button of the section headers ("Show all ›", "View: Grid ⌄"): Windows shows the
// standard button fill only under the pointer.
PlasmaComponents3.ToolButton {
    id: button

    property string glyph: "arrow-right"

    implicitHeight: 32
    leftPadding: 12
    rightPadding: 11
    topPadding: 0
    bottomPadding: 0
    Accessible.name: text

    contentItem: Row {
        spacing: 8
        layoutDirection: button.glyph === "arrow-left" ? Qt.RightToLeft : Qt.LeftToRight
        PlasmaComponents3.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: button.text
            font.pixelSize: lt.typeBody[0]
        }
        Kirigami.Icon {
            anchors.verticalCenter: parent.verticalCenter
            roundToIconSize: false
            width: 12
            height: 12
            source: button.glyph
            color: lt.textSecondary
            isMask: true
        }
    }
    background: Rectangle {
        radius: lt.radiusControl
        color: button.down ? lt.controlFillPressed : lt.controlFill
        border.width: button.visualFocus ? 2 : 0
        border.color: lt.focusOuter
        opacity: button.hovered || button.down || button.visualFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }
}
