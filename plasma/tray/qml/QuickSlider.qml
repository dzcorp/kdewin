import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

// Win11 Quick Settings slider row: icon button, Fluent slider, optional details chevron.
RowLayout {
    id: row

    property string iconName
    property string iconToolTip
    property real value: 0
    property real to: 100
    property real stepSize: 1
    property bool available: true
    property bool hasDetails: false
    property bool showValue: true

    signal moved(real value)
    signal iconClicked()
    signal detailsRequested()

    spacing: 4
    enabled: available
    opacity: available ? 1 : 0.5

    PlasmaComponents3.ToolButton {
        icon.name: row.iconName
        display: PlasmaComponents3.AbstractButton.IconOnly
        text: row.iconToolTip
        PlasmaComponents3.ToolTip.text: row.iconToolTip
        PlasmaComponents3.ToolTip.visible: hovered && row.iconToolTip !== ""
        onClicked: row.iconClicked()
    }

    T.Slider {
        id: slider
        Layout.fillWidth: true
        implicitHeight: lt.sizeControlH
        from: 0
        to: row.to
        stepSize: row.stepSize
        live: true
        wheelEnabled: true
        focusPolicy: Qt.StrongFocus
        Binding on value {
            when: !slider.pressed
            value: row.value
            restoreMode: Binding.RestoreNone
        }
        onMoved: row.moved(value)

        background: Item {
            x: slider.leftPadding
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth
            height: lt.sizeSliderTrack
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: lt.controlStrongFill
            }
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: height / 2
                color: slider.enabled ? lt.accentFill : lt.accentDisabled
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: lt.sizeSliderThumb
            height: width
            radius: width / 2
            color: lt.controlSolidFill
            border.width: 1
            border.color: lt.controlStroke

            Rectangle {
                anchors.centerIn: parent
                property real d: slider.pressed ? lt.sizeSliderThumbInnerPressed : slider.hovered ? lt.sizeSliderThumbInnerHover : lt.sizeSliderThumbInner
                width: d
                height: d
                radius: d / 2
                color: slider.enabled ? lt.accentFill : lt.accentDisabled
                Behavior on d { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingPoint } }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 6
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: lt.focusOuter
                visible: slider.visualFocus
            }
        }
    }

    PlasmaComponents3.Label {
        visible: row.showValue
        Layout.minimumWidth: valueMetrics.width
        horizontalAlignment: Text.AlignRight
        text: Math.round(slider.value)
        color: lt.textSecondary
        font.pixelSize: lt.typeCaption[0]
        TextMetrics {
            id: valueMetrics
            text: "100"
        }
    }

    PlasmaComponents3.ToolButton {
        visible: row.hasDetails
        icon.name: Application.layoutDirection === Qt.RightToLeft ? "arrow-left" : "arrow-right"
        display: PlasmaComponents3.AbstractButton.IconOnly
        text: i18n("More options")
        onClicked: row.detailsRequested()
    }
}
