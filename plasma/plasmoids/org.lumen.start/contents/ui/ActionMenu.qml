import QtQuick
import org.kde.plasma.extras as PlasmaExtras

// Context menu built from a kicker actionList (+ Lumen pin actions).
Item {
    id: holder

    property var model
    property int row: -1

    function show(target: Item, model, row: int, actions: var, x: real, y: real) {
        holder.model = model;
        holder.row = row;
        menu.clearMenuItems();
        for (const action of actions) {
            if (action.type === "separator") {
                menu.addMenuItem(separator.createObject(menu), null);
                continue;
            }
            if (action.type === "title") {
                continue;
            }
            const item = entry.createObject(menu, {
                text: action.text ?? "",
                icon: action.icon ?? "",
                checkable: action.checkable ?? false,
                checked: action.checked ?? false,
                enabled: !(action.disabled ?? false),
            });
            item.clicked.connect(() => start.triggerAction(holder.model, holder.row, action.actionId, action.actionArgument));
            menu.addMenuItem(item, null);
        }
        menu.visualParent = target;
        menu.open(x, y);
    }

    PlasmaExtras.Menu {
        id: menu
        placement: PlasmaExtras.Menu.BottomPosedLeftAlignedPopup
    }

    Component {
        id: entry
        PlasmaExtras.MenuItem {}
    }

    Component {
        id: separator
        PlasmaExtras.MenuItem { separator: true }
    }
}
