import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents3

// Notification Center panel (Windows: title, Do not disturb, Clear all; app groups of cards; or a short empty panel).
ColumnLayout {
    id: panel

    readonly property alias rows: notes.rows

    function positionViewAtBeginning() {
        notes.positionViewAtBeginning();
    }

    spacing: 0
    Keys.onEscapePressed: root.expanded = false

    HoverHandler { id: hover }
    Binding {
        target: root
        property: "centerHovered"
        value: hover.hovered
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 4
        Layout.topMargin: 1
        Layout.bottomMargin: 8
        Layout.preferredHeight: 24
        spacing: 8

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: i18n("Notifications")
            font.pixelSize: lt.typeBody[0]
        }
        PlasmaComponents3.Button {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            leftPadding: 4
            rightPadding: 4
            topPadding: 4
            bottomPadding: 4
            icon.name: root.dnd ? "notifications-disabled-symbolic" : "notifications-symbolic"
            icon.width: 16
            icon.height: 16
            checkable: true
            checked: root.dnd
            display: PlasmaComponents3.AbstractButton.IconOnly
            text: i18n("Do not disturb")
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.visible: hovered
            onClicked: root.toggleDnd()
        }
        PlasmaComponents3.Button {
            visible: center.count > 0
            Layout.preferredHeight: 24
            leftPadding: 10
            rightPadding: 10
            topPadding: 0
            bottomPadding: 0
            text: i18n("Clear all")
            font.pixelSize: lt.typeCaption[0]
            onClicked: notes.leave("", root.clearAll)
        }
    }

    PlasmaComponents3.Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: notes.rows === 0
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: root.dnd ? i18n("Do not disturb is on") : i18n("No new notifications")
        color: lt.textSecondary
        font.pixelSize: lt.typeBody[0]
    }

    // Cards sit 4 px inside the panel border, as in Windows.
    NotificationView {
        id: notes
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 5 - lt.sizeFlyoutPad
        Layout.rightMargin: 5 - lt.sizeFlyoutPad
        visible: rows > 0
    }
}
