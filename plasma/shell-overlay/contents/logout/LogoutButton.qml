import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "timer.js" as AutoTriggerTimer

// Lumen: Fluent tile (icon over label) replacing Breeze's round action button; same API.
QQC2.AbstractButton {
    id: button

    Layout.alignment: Qt.AlignTop
    implicitWidth: 112
    implicitHeight: 104
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    icon.width: 32
    icon.height: 32
    Accessible.name: text.replace("&", "")

    Keys.onPressed: event => {
        AutoTriggerTimer.cancelAutoTrigger();
        event.accepted = false;
    }
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onSpacePressed: clicked()

    background: Rectangle {
        radius: 8
        color: button.down ? Qt.rgba(1, 1, 1, 0.06) : button.hovered || button.activeFocus ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
        border.width: button.activeFocus ? 2 : 1
        border.color: button.activeFocus ? "#ffffff" : Qt.rgba(1, 1, 1, 0.09)
        Behavior on color { ColorAnimation { duration: 83 } }
    }

    contentItem: ColumnLayout {
        spacing: 10
        Kirigami.Icon {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: button.icon.width
            Layout.preferredHeight: button.icon.height
            source: button.icon.name
            color: "#ffffff"
        }
        QQC2.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: button.text.replace("&", "")
            color: "#ffffff"
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
