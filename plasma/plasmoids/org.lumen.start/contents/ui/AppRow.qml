import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// List row (all apps, search results, recommended).
Item {
    id: rowItem

    required property var model
    required property int index
    property Item view
    property int iconSize: 24
    property int rowHeight: 0
    property bool showSubtitle: true
    // Caption-size title, as in Recommended.
    property bool small: false
    property string subtitle: model.description ?? ""

    readonly property bool current: view && view.currentIndex === index && (view.activeFocus || view.showCurrent === true)
    readonly property bool dragging: dragHandler.active

    implicitHeight: rowHeight > 0 ? rowHeight : Math.max(lt.sizeListItemH, iconSize + 12)
    width: ListView.view ? ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin : (GridView.view ? GridView.view.cellWidth : implicitWidth)
    opacity: start.dragSource.sourceItem === rowItem ? 0.4 : 1
    Accessible.name: model.display
    Accessible.description: subtitle
    Accessible.role: Accessible.ListItem
    onCurrentChanged: if (current) menu.reveal(rowItem)

    Rectangle {
        anchors.fill: parent
        radius: lt.radiusControl
        color: mouse.pressed ? lt.controlFillPressed : lt.controlFill
        opacity: mouse.containsMouse || rowItem.current ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
    }

    Rectangle {
        visible: rowItem.current
        anchors.verticalCenter: parent.verticalCenter
        x: 1
        width: lt.sizeListPillW
        height: lt.sizeListPillH
        radius: lt.radiusListPill
        color: lt.accentFill
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        Kirigami.Icon {
            id: icon
            roundToIconSize: false
            Layout.preferredWidth: rowItem.iconSize
            Layout.preferredHeight: rowItem.iconSize
            source: rowItem.model.decoration
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            PlasmaComponents3.Label {
                id: title
                Layout.fillWidth: true
                text: rowItem.model.display
                elide: Text.ElideRight
                textFormat: Text.PlainText
                Binding on font.pixelSize {
                    when: rowItem.small
                    value: lt.typeCaption[0]
                }
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                visible: rowItem.showSubtitle && text !== ""
                text: rowItem.subtitle
                elide: Text.ElideMiddle
                color: lt.textSecondary
                font.pixelSize: lt.typeCaption[0]
                textFormat: Text.PlainText
            }
        }
        Rectangle {
            visible: rowItem.model.isNewlyInstalled ?? false
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            radius: 3
            color: lt.accentFill
        }
    }

    DragHandler {
        id: dragHandler
        target: null
        enabled: (rowItem.model.url?.toString() ?? "") !== ""
        onActiveChanged: start.drag(active, rowItem, icon)
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: event => {
            if (event.button === Qt.RightButton) {
                start.openMenu(rowItem, rowItem.view.model, rowItem.index, rowItem.view.pinnable ?? true, event.x, event.y);
            } else {
                start.launch(rowItem.view.model, rowItem.index);
            }
        }
        onPressAndHold: event => start.openMenu(rowItem, rowItem.view.model, rowItem.index, rowItem.view.pinnable ?? true, event.x, event.y)
    }
}
