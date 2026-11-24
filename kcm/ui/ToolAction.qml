import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormButtonDelegate {
    property var args: []
    property bool asRoot: false

    enabled: kcm.toolAvailable && !kcm.busy
    icon.name: asRoot ? "security-medium-symbolic" : "system-run-symbolic"
    onClicked: kcm.run(args, asRoot)
}
