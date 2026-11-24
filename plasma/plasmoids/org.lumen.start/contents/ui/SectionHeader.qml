import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3

RowLayout {
    id: header

    property alias text: label.text
    property alias action: button.text
    property string icon: "arrow-right"
    // Extra controls placed before the action button.
    default property alias extras: extraRow.data
    signal triggered()

    Layout.fillWidth: true
    Layout.leftMargin: menu.pad + 32
    Layout.rightMargin: menu.pad + 20
    Layout.preferredHeight: 32
    spacing: 8

    PlasmaComponents3.Label {
        id: label
        Layout.fillWidth: true
        elide: Text.ElideRight
        font.pixelSize: lt.typeBodyStrong[0]
        font.weight: lt.typeBodyStrong[2]
    }
    RowLayout {
        id: extraRow
        spacing: 4
    }
    LinkButton {
        id: button
        visible: text !== ""
        glyph: header.icon
        onClicked: header.triggered()
    }
}
