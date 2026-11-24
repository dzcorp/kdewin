import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormSwitchDelegate {
    required property string setting

    checked: kcm.settings[setting]
    onToggled: kcm.settings[setting] = checked
}
