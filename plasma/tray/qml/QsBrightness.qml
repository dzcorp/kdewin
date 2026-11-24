pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.private.brightnesscontrolplugin

// One slider per display (internal panel and DDC/CI monitors).
ColumnLayout {
    ScreenBrightnessControl {
        id: control
        isSilent: false
    }

    readonly property bool available: control.isBrightnessAvailable

    spacing: 8

    Repeater {
        model: control.displays
        delegate: QuickSlider {
            required property int index
            required property string displayName
            required property string label
            required property int brightness
            required property int maxBrightness

            Layout.fillWidth: true
            iconName: "brightness-high-symbolic"
            iconToolTip: label
            value: maxBrightness > 0 ? brightness / maxBrightness * 100 : 0
            hasDetails: index === 0
            onMoved: v => control.setBrightness(displayName, Math.max(1, Math.round(v / 100 * maxBrightness)))
            onDetailsRequested: qs.openPage("org.kde.plasma.brightness")
        }
    }
}
