import QtQuick
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

// "View: List ▾" for All apps; Category appears once enabled in the settings.
LinkButton {
    id: picker

    required property int mode
    readonly property var names: [i18n("List"), i18n("Grid"), i18n("Category")]

    Accessible.role: Accessible.ButtonMenu
    text: i18nc("@action:button %1 is List, Grid or Category", "View: %1", names[mode])
    glyph: "arrow-down"
    onClicked: viewMenu.open(0, height)

    PlasmaExtras.Menu {
        id: viewMenu
        visualParent: picker
        placement: PlasmaExtras.Menu.BottomPosedRightAlignedPopup
        PlasmaExtras.MenuItem {
            text: picker.names[0]
            icon: "view-list-details-symbolic"
            checkable: true
            checked: picker.mode === 0
            onClicked: Plasmoid.configuration.appsView = 0
        }
        PlasmaExtras.MenuItem {
            text: picker.names[1]
            icon: "view-list-icons-symbolic"
            checkable: true
            checked: picker.mode === 1
            onClicked: Plasmoid.configuration.appsView = 1
        }
        PlasmaExtras.MenuItem {
            text: picker.names[2]
            icon: "view-group-symbolic"
            checkable: true
            checked: picker.mode === 2
            visible: Plasmoid.configuration.showCategories
            onClicked: Plasmoid.configuration.appsView = 2
        }
    }
}
