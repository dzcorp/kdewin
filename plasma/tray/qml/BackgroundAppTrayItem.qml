pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.extras as PlasmaExtras

// Portal background apps (xdg-desktop-portal BackgroundApps).
TrayItem {
    id: app

    text: model.name
    mainText: model.name
    subText: model.message
    active: true

    TrayIcon {
        parent: app.iconContainer
        source: app.model.icon
        active: app.hovered
    }

    function sourceIndex() {
        const view = app.parent?.ListView.view ?? app.parent?.GridView.view ?? null;
        let index = view?.model.index(app.index, 0) ?? null;
        while (index?.valid && index.model.hasOwnProperty("mapToSource")) {
            index = index.model.mapToSource(index);
        }
        return index;
    }

    onActivated: pos => {
        const index = sourceIndex();
        index?.model.openBackgroundApp(index);
    }
    onClicked: mouse => {
        if (mouse.button === Qt.LeftButton) {
            activated(null);
        } else if (mouse.button === Qt.RightButton) {
            menu.open(mouse.x, mouse.y);
        }
    }
    onContextMenu: mouse => menu.open(mouse?.x ?? width / 2, mouse?.y ?? height / 2)

    PlasmaExtras.Menu {
        id: menu
        visualParent: app
        PlasmaExtras.MenuItem {
            text: i18nc("@action:inmenu", "Quit %1", app.model.name)
            icon: "application-exit"
            onClicked: {
                const index = app.sourceIndex();
                index?.model.stopBackgroundApp(index);
            }
        }
    }
}
