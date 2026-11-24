import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormSliderDelegate {
    required property string setting
    property string suffix: "%"

    stepSize: [1, 2, 5, 10, 25].find(step => (to - from) / step <= 25) ?? 25
    value: kcm.settings[setting]
    valueText: Math.round(value) + suffix
    onMoved: kcm.settings[setting] = Math.round(value)
}
