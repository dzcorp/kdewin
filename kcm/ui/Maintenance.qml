import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    title: i18n("Maintenance")

    LumenCard {
        visible: !kcm.toolAvailable
        FormCard.FormTextDelegate {
            icon.name: "dialog-warning"
            text: i18n("The lumen tool was not found")
            description: i18n("Install it with `make install-system` from the Lumen source folder.")
        }
    }

    LumenHeader { title: i18n("Check") }
    LumenCard {
        ToolAction { text: i18n("Run diagnostics"); description: i18n("Checks installed components and whether the materials effect is loaded"); args: ["doctor"] }
        ToolAction { text: i18n("Show the panel layout snapshot"); args: ["snapshot"] }
    }

    LumenHeader { title: i18n("Repair") }
    LumenCard {
        ToolAction { text: i18n("Re-apply Lumen"); description: i18n("Colors, style, decoration, effects and Plasma theme"); args: ["apply", kcm.settings.appearanceMode] }
        ToolAction { text: i18n("Restart desktop components"); description: i18n("Use after changing icons or themes in System Settings: refreshes icons in open apps and reloads the taskbar, desktop and search"); args: ["restart"] }
        ToolAction { text: i18n("Find replaced tools"); description: i18n("Lists installed packages Lumen replaces, such as Kvantum and Klassy. Remove them with `lumen cleanup` in a terminal."); args: ["cleanup"] }
        ToolAction { text: i18n("Rebuild native parts"); description: i18n("Runs automatically after KWin or Qt updates"); args: ["rebuild-kwin"]; asRoot: true }
    }

    LumenCard {
        FormCard.FormTextDelegate {
            icon.name: "help-about"
            text: "Lumen " + kcm.version
        }
    }

    ToolLog {}
}
