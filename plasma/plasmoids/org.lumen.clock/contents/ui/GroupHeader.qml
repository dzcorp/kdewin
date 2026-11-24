import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// App header of a notification group: expand to show all of the app's notifications, or clear them at once.
Item {
    id: header

    required property var model
    readonly property bool expandable: model?.groupHidden > 0 || (model?.groupExpanded ?? false)

    signal clearRequested()

    // Room for the divider above every group but the first; it eases away when a group becomes first.
    property real gap: model?.firstGroup ? 0 : 9
    Behavior on gap { NumberAnimation { duration: lt.motionSlowMs; easing.type: Easing.InOutCubic } }

    implicitHeight: 44 + gap

    HoverHandler { id: hover }

    Rectangle {
        opacity: header.gap / 9
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 8; rightMargin: 8; topMargin: 4 }
        height: 1
        color: lt.divider
    }

    RowLayout {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 12; rightMargin: 4 }
        height: 44
        spacing: 8

        Kirigami.Icon {
            roundToIconSize: false
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            // Apps without a desktop entry only name their icon per notification.
            source: header.model?.applicationIconName || header.model?.iconName || "dialog-information"
        }
        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: header.model?.applicationName || i18n("Other")
            elide: Text.ElideRight
            font.pixelSize: lt.typeCaption[0]
        }
        // Windows' "+1 notifications" chip: a standard button.
        PlasmaComponents3.Button {
            visible: header.expandable
            Layout.preferredHeight: 24
            leftPadding: 8
            rightPadding: 8
            topPadding: 0
            bottomPadding: 0
            font.pixelSize: lt.typeCaption[0]
            text: header.model?.groupExpanded ? i18n("Show less") : i18np("%1 more", "%1 more", header.model?.groupHidden)
            onClicked: center.setExpanded(header.model?.groupKey, !header.model?.groupExpanded)
        }
        // Windows shows it while the header is hovered.
        PlasmaComponents3.ToolButton {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            opacity: hover.hovered || activeFocus ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
            icon.name: "window-close-symbolic"
            display: PlasmaComponents3.AbstractButton.IconOnly
            text: i18n("Clear group")
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.visible: hovered
            onClicked: header.clearRequested()
        }
    }
}
