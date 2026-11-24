pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

// Recent files (and apps, if enabled) in two columns under a divider. `fill` shows as many rows as fit.
ColumnLayout {
    id: section

    property bool fill: true
    property int rows: 2
    readonly property alias view: grid
    signal more()

    spacing: 0
    visible: Plasmoid.configuration.showRecommended

    Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: menu.pad
        Layout.rightMargin: menu.pad
        Layout.preferredHeight: 1
        color: lt.divider
    }

    SectionHeader {
        Layout.topMargin: 14
        text: i18n("Recommended")
        action: grid.count > grid.limit ? i18n("More") : ""
        onTriggered: section.more()
    }

    GridView {
        id: grid
        readonly property bool pinnable: false
        readonly property int shownRows: section.fill ? Math.max(1, Math.floor(height / cellHeight)) : section.rows
        readonly property int limit: shownRows * 2
        readonly property int lastIndex: Math.min(count, limit) - 1

        Layout.fillWidth: true
        Layout.fillHeight: section.fill
        Layout.topMargin: 2
        Layout.preferredHeight: section.fill ? -1 : Math.max(1, Math.min(section.rows, Math.ceil(count / 2))) * cellHeight
        // Windows: rows aligned with the list view (icon 64 px from the menu edge), 48 px tall.
        Layout.leftMargin: menu.pad + 20
        Layout.rightMargin: menu.pad + 20
        clip: true
        interactive: false
        cellWidth: Math.floor(width / 2)
        cellHeight: 48
        model: start.recentModel
        keyNavigationEnabled: true
        currentIndex: -1
        activeFocusOnTab: true
        Accessible.name: i18n("Recommended")
        onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
        Component.onCompleted: menu.register(grid)
        Component.onDestruction: menu.unregister(grid)

        Keys.onReturnPressed: start.launch(model, currentIndex)
        Keys.onEnterPressed: start.launch(model, currentIndex)
        Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, false, 0, 0)
        Keys.onPressed: event => {
            const i = currentIndex;
            if (event.key === Qt.Key_Up && i < 2) {
                menu.step(grid, -1);
                event.accepted = true;
            } else if (event.key === Qt.Key_Down && i + 2 > lastIndex) {
                if (Math.floor(i / 2) < Math.floor(lastIndex / 2)) {
                    currentIndex = lastIndex;
                } else {
                    menu.step(grid, 1);
                }
                event.accepted = true;
            } else if (event.key === Qt.Key_Right && i + 1 > lastIndex) {
                event.accepted = true;
            }
        }

        delegate: AppRow {
            view: grid
            width: grid.cellWidth
            height: grid.cellHeight
            iconSize: 32
            small: true
            visible: index < grid.limit
        }

        PlasmaExtras.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - 48
            visible: grid.count === 0
            text: i18n("Your recent files will show up here")
        }
    }
}
