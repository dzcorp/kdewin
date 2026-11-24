import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Boot")

    LumenCard {
        ToolAction {
            text: i18n("Use Lumen for boot")
            description: i18n("Limine boot menu and Plymouth splash. The previous menu is backed up.")
            args: ["boot", "on"]
            asRoot: true
        }
        ToolAction {
            text: i18n("Restore the previous boot look")
            args: ["boot", "off"]
            asRoot: true
        }
    }

    ToolLog {}
}
