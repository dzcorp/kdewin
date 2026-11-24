// WinUI 3 ToggleSwitch: 40x20 pill, 12 px knob growing to 14 on hover and stretching to 17 while pressed.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.lumen.settings as Lumen

Item {
    id: indicator

    property T.AbstractButton control
    readonly property bool on: control.checked
    readonly property bool hot: control.enabled && control.hovered
    readonly property bool down: control.enabled && control.pressed

    implicitWidth: 40
    implicitHeight: 20
    layer.enabled: control.opacity < 1.0

    LumenTokens {
        id: lt
        reference: Kirigami.Theme.backgroundColor
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: !indicator.control.enabled ? (indicator.on ? lt.accentDisabled : "transparent")
             : indicator.on ? (indicator.down ? lt.accentFillPressed : indicator.hot ? lt.accentFillHover : lt.accentFill)
             : (indicator.down ? lt.controlAltFillPressed : indicator.hot ? lt.controlAltFillHover : lt.controlAltFill)
        border.width: indicator.on ? 0 : 1
        border.color: indicator.control.enabled ? lt.controlStrongStroke : lt.controlStrongStrokeDisabled
        Behavior on color {
            ColorAnimation { duration: lt.motionFasterMs * Lumen.Settings.animationScale }
        }
    }

    Rectangle {
        readonly property real size: indicator.down || indicator.hot ? 14 : 12
        width: indicator.down ? 17 : size
        height: size
        radius: height / 2
        // Knob centre travels from 10 to 30; while pressed it stretches toward the middle.
        x: Math.round(10 + 20 * indicator.control.visualPosition - width / 2 + (indicator.down ? (indicator.control.visualPosition < 0.5 ? 1.5 : -1.5) : 0))
        y: (parent.height - height) / 2
        color: !indicator.control.enabled ? (indicator.on ? lt.textOnAccentDisabled : lt.textDisabled)
             : indicator.on ? lt.textOnAccent : lt.textSecondary
        Behavior on x {
            enabled: !indicator.control.pressed
            NumberAnimation { duration: lt.motionFastMs * Lumen.Settings.animationScale; easing.type: Easing.OutCubic }
        }
        Behavior on width {
            NumberAnimation { duration: lt.motionFasterMs * Lumen.Settings.animationScale }
        }
        Behavior on height {
            NumberAnimation { duration: lt.motionFasterMs * Lumen.Settings.animationScale }
        }
    }
}
