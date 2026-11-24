import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// App tile: 32 px icon over a two-line label (pinned, all-apps grid, category folder).
Item {
    id: tile

    required property var model
    required property int index
    property Item view

    readonly property bool current: view && view.currentIndex === index && (view.activeFocus || view.showCurrent === true)
    readonly property bool dragging: dragHandler.active

    width: GridView.view.cellWidth
    height: GridView.view.cellHeight
    opacity: start.dragSource.sourceItem === tile ? 0.4 : 1
    Accessible.name: model.display
    Accessible.role: Accessible.Button
    onCurrentChanged: if (current) menu.reveal(tile)

    Rectangle {
        anchors.fill: parent
        radius: lt.radiusTile
        color: mouse.pressed ? lt.controlFillPressed : lt.controlFill
        opacity: mouse.containsMouse || tile.current ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: lt.radiusTile + 1
        color: "transparent"
        border.width: 2
        border.color: lt.focusOuter
        visible: tile.current && tile.view.activeFocus
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        anchors.topMargin: 12
        spacing: 5

        Kirigami.Icon {
            id: icon
            roundToIconSize: false
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: tile.model.decoration
            scale: mouse.pressed ? 0.9 : 1
            Behavior on scale { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }

            Rectangle {
                visible: tile.model.isNewlyInstalled ?? false
                anchors { right: parent.right; top: parent.top; margins: -2 }
                width: 6
                height: 6
                radius: 3
                color: lt.accentFill
            }
        }
        PlasmaComponents3.Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            text: tile.model.display
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            textFormat: Text.PlainText
            font.pixelSize: lt.typeCaption[0]
        }
    }

    DragHandler {
        id: dragHandler
        target: null
        enabled: (tile.model.url?.toString() ?? "") !== ""
        onActiveChanged: start.drag(active, tile, icon)
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: event => {
            if (event.button === Qt.RightButton) {
                start.openMenu(tile, tile.view.model, tile.index, tile.view.pinnable ?? true, event.x, event.y);
            } else {
                start.launch(tile.view.model, tile.index);
            }
        }
        onPressAndHold: event => start.openMenu(tile, tile.view.model, tile.index, tile.view.pinnable ?? true, event.x, event.y)
    }
}
