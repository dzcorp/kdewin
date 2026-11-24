pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

// All apps as a list (letter headers jump), an icon grid, or KDE menu categories opening as folders.
// `header` stacks the home sections above the apps (single-page layout).
FocusScope {
    id: apps

    property Component header: null
    property bool showTitle: false
    readonly property int mode: Plasmoid.configuration.appsView === 2 && !Plasmoid.configuration.showCategories ? 0 : Math.max(0, Math.min(2, Plasmoid.configuration.appsView))
    readonly property var appsModel: start.rootModel.modelForRow(start.appsRow)
    readonly property Item view: loader.item
    property bool letters: false
    property var folder: null

    function reset() {
        letters = false;
        folder = null;
        if (view) {
            view.currentIndex = -1;
            view.positionViewAtBeginning();
        }
    }

    function back(): bool {
        if (!letters && !folder) {
            return false;
        }
        letters = false;
        folder = null;
        view?.forceActiveFocus();
        return true;
    }

    function jump(row: int) {
        letters = false;
        view.currentIndex = row;
        view.positionViewAtIndex(row, ListView.Beginning);
        view.forceActiveFocus();
    }

    function openFolder(name: string, members, keyboard: bool) {
        folder = { name, members };
        folderGrid.currentIndex = keyboard ? 0 : -1;
        folderGrid.forceActiveFocus();
    }

    Connections {
        target: menu
        function onReset() { apps.reset(); }
    }

    KItemModels.KSortFilterProxyModel {
        id: recentlyAdded
        sourceModel: Plasmoid.configuration.showRecentlyAdded && apps.mode !== 2 ? apps.appsModel : null
        filterRoleName: "isNewlyInstalled"
        filterString: "true"
    }

    component MiniList: ColumnLayout {
        id: mini
        property alias title: miniHeader.text
        property alias model: list.model
        property int limit: 3
        readonly property alias count: list.count

        spacing: 4

        SectionHeader { id: miniHeader }

        ListView {
            id: list
            readonly property bool pinnable: true
            readonly property int lastIndex: Math.min(count, mini.limit) - 1

            Layout.fillWidth: true
            Layout.leftMargin: menu.pad
            Layout.rightMargin: menu.pad
            Layout.preferredHeight: Math.min(count, mini.limit) * 40
            interactive: false
            keyNavigationEnabled: true
            currentIndex: -1
            onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
            Component.onCompleted: menu.register(list)
            Component.onDestruction: menu.unregister(list)
            Keys.onReturnPressed: start.launch(model, currentIndex)
            Keys.onEnterPressed: start.launch(model, currentIndex)
            Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
            Keys.onUpPressed: event => {
                if (currentIndex <= 0) {
                    menu.step(list, -1);
                } else {
                    event.accepted = false;
                }
            }
            Keys.onDownPressed: event => {
                if (currentIndex >= lastIndex) {
                    menu.step(list, 1);
                } else {
                    event.accepted = false;
                }
            }
            delegate: AppRow {
                view: list
                rowHeight: 40
                showSubtitle: false
                visible: index < mini.limit
            }
        }
    }

    component CategoryCard: Item {
        id: card

        required property int index
        required property var model
        property var proxy
        readonly property var members: start.rootModel.modelForRow(proxy.mapToSource(proxy.index(index, 0)).row)
        readonly property bool current: GridView.view.currentIndex === index && GridView.view.activeFocus
        signal opened(string name, var members, bool keyboard)

        width: GridView.view.cellWidth
        height: GridView.view.cellHeight
        Accessible.role: Accessible.Button
        Accessible.name: model.display
        onCurrentChanged: if (current) menu.reveal(card)

        Rectangle {
            id: square
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(card.width - 22, 156)
            height: width
            radius: lt.radiusOverlay
            color: squareMouse.pressed ? lt.controlFillPressed : squareMouse.containsMouse ? lt.controlFill : lt.cardBgSecondary
            border.width: card.current ? 2 : 1
            border.color: card.current ? lt.focusOuter : lt.cardStroke
            Behavior on color { ColorAnimation { duration: lt.motionFasterMs } }

            Grid {
                anchors.centerIn: parent
                columns: 2
                spacing: 36
                Repeater {
                    model: Math.min(4, card.members?.count ?? 0)
                    delegate: Kirigami.Icon {
                        required property int index
                        width: 32
                        height: 32
                        roundToIconSize: false
                        source: card.members.data(card.members.index(index, 0), Qt.DecorationRole)
                    }
                }
            }
        }

        PlasmaComponents3.Label {
            anchors { top: square.bottom; topMargin: 14; left: parent.left; right: parent.right; leftMargin: 4; rightMargin: 4 }
            horizontalAlignment: Text.AlignHCenter
            text: card.model.display
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            textFormat: Text.PlainText
            font.pixelSize: lt.typeCaption[0]
        }

        MouseArea {
            id: squareMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: card.opened(card.model.display, card.members, false)
        }
    }

    // Items in a view header get the view's margin as offset; undo it so sections span the menu width.
    Component {
        id: headerComponent
        Item {
            id: head
            readonly property Item flick: parent?.parent ?? null
            property real lastHeight: 0
            width: flick ? flick.width - flick.leftMargin - flick.rightMargin : 0
            height: column.implicitHeight
            // The header grows upwards from the first item; stay at the top if the view was there.
            onHeightChanged: {
                if (flick && flick.contentY <= -lastHeight + 1) {
                    flick.contentY = -height;
                }
                lastHeight = height;
            }

            ColumnLayout {
                id: column
                x: -(head.flick?.leftMargin ?? 0)
                width: head.flick?.width ?? 0
                spacing: 8

                Loader {
                    Layout.fillWidth: true
                    active: apps.header !== null
                    visible: active
                    sourceComponent: apps.header
                }
                SectionHeader {
                    visible: apps.showTitle
                    Layout.topMargin: 8
                    text: i18n("All")
                    ViewPicker { mode: apps.mode }
                }
                MiniList {
                    Layout.fillWidth: true
                    visible: count > 0
                    title: i18n("Recently added")
                    model: recentlyAdded.sourceModel ? recentlyAdded : null
                    limit: 3
                }
                MiniList {
                    Layout.fillWidth: true
                    visible: count > 0 && apps.mode !== 2
                    title: i18n("Most used")
                    model: start.mostUsedModel
                    limit: 6
                }
                Item { Layout.preferredHeight: 4 }
            }
        }
    }

    Component {
        id: letterHeader
        Item {
            id: letterItem
            required property string section
            width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
            height: 40
            Accessible.role: Accessible.Button
            Accessible.name: section
            Accessible.description: i18n("Jump to a letter")

            Rectangle {
                id: letterButton
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                width: Math.max(32, letterLabel.implicitWidth + 16)
                height: 32
                radius: lt.radiusControl
                color: letterMouse.pressed ? lt.subtleFillPressed : lt.subtleFillHover
                opacity: letterMouse.containsMouse ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
            }
            PlasmaComponents3.Label {
                id: letterLabel
                anchors.centerIn: letterButton
                text: letterItem.section.toUpperCase()
                font.weight: Font.DemiBold
                color: lt.textSecondary
            }
            MouseArea {
                id: letterMouse
                anchors.fill: letterButton
                hoverEnabled: true
                onClicked: apps.letters = true
            }
        }
    }

    Component {
        id: listComponent
        ListView {
            id: list
            readonly property bool pinnable: true
            model: apps.appsModel
            leftMargin: menu.pad + 20
            rightMargin: menu.pad + 20
            clip: true
            reuseItems: true
            keyNavigationEnabled: true
            activeFocusOnTab: true
            currentIndex: -1
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            header: headerComponent
            section.property: "group"
            section.delegate: letterHeader
            PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}
            onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
            Component.onCompleted: menu.register(list)
            Component.onDestruction: menu.unregister(list)
            Keys.onReturnPressed: start.launch(model, currentIndex)
            Keys.onEnterPressed: start.launch(model, currentIndex)
            Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
            Keys.onUpPressed: event => {
                if (currentIndex <= 0) {
                    menu.step(list, -1);
                } else {
                    event.accepted = false;
                }
            }
            delegate: AppRow {
                view: list
                rowHeight: 40
                iconSize: 32
                showSubtitle: false
            }
        }
    }

    Component {
        id: gridComponent
        GridView {
            id: grid
            readonly property bool pinnable: true
            model: apps.appsModel
            leftMargin: menu.pad
            rightMargin: menu.pad
            clip: true
            reuseItems: true
            keyNavigationEnabled: true
            activeFocusOnTab: true
            currentIndex: -1
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor((width - leftMargin - rightMargin) / menu.cols)
            cellHeight: menu.tileH
            header: headerComponent
            PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}
            onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
            Component.onCompleted: menu.register(grid)
            Component.onDestruction: menu.unregister(grid)
            Keys.onReturnPressed: start.launch(model, currentIndex)
            Keys.onEnterPressed: start.launch(model, currentIndex)
            Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
            Keys.onUpPressed: event => {
                if (currentIndex < menu.cols) {
                    menu.step(grid, -1);
                } else {
                    event.accepted = false;
                }
            }
            delegate: AppTile { view: grid }
        }
    }

    Component {
        id: categoryComponent
        GridView {
            id: cats
            readonly property bool pinnable: false
            readonly property int columns: menu.large ? 4 : 3
            model: KItemModels.KSortFilterProxyModel {
                id: categoryModel
                sourceModel: start.rootModel
                filterRowCallback: (row, parent) => {
                    const members = start.rootModel.modelForRow(row);
                    return !!members && members.count > 0 && members.description !== "KICKER_ALL_MODEL" && members.description !== "KICKER_FAVORITES_MODEL";
                }
            }
            leftMargin: menu.pad + 21
            rightMargin: menu.pad + 21
            clip: true
            keyNavigationEnabled: true
            activeFocusOnTab: true
            currentIndex: -1
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor((width - leftMargin - rightMargin) / columns)
            cellHeight: 204
            header: headerComponent
            PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}
            onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
            Component.onCompleted: menu.register(cats)
            Component.onDestruction: menu.unregister(cats)
            Keys.onReturnPressed: currentItem && apps.openFolder(currentItem.model.display, currentItem.members, true)
            Keys.onEnterPressed: currentItem && apps.openFolder(currentItem.model.display, currentItem.members, true)
            Keys.onUpPressed: event => {
                if (currentIndex < columns) {
                    menu.step(cats, -1);
                } else {
                    event.accepted = false;
                }
            }
            delegate: CategoryCard {
                proxy: categoryModel
                onOpened: (name, members, keyboard) => apps.openFolder(name, members, keyboard)
            }
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        focus: true
        opacity: apps.letters || apps.folder ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }
        sourceComponent: [listComponent, gridComponent, categoryComponent][apps.mode]
    }

    // Letter index: every first letter present, as in Windows.
    GridView {
        id: letterGrid
        readonly property int columns: Math.min(7, Math.max(1, count))
        anchors.centerIn: parent
        width: columns * cellWidth
        height: Math.min(parent.height - 24, Math.ceil(count / columns) * cellHeight)
        cellWidth: 56
        cellHeight: 56
        clip: true
        interactive: contentHeight > height
        opacity: apps.letters ? 1 : 0
        scale: apps.letters ? 1 : 1.06
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }
        Behavior on scale { NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel } }
        model: apps.letters ? apps.appsModel?.sections ?? null : null
        keyNavigationEnabled: true
        currentIndex: 0
        onVisibleChanged: if (visible) forceActiveFocus()
        Keys.onReturnPressed: currentItem && apps.jump(currentItem.firstIndex)
        Keys.onEnterPressed: currentItem && apps.jump(currentItem.firstIndex)
        delegate: Item {
            id: letter
            required property string section
            required property int firstIndex
            required property int index
            readonly property bool current: letterGrid.currentIndex === index && letterGrid.activeFocus
            width: letterGrid.cellWidth
            height: letterGrid.cellHeight
            Accessible.role: Accessible.Button
            Accessible.name: section

            Rectangle {
                anchors { fill: parent; margins: 4 }
                radius: lt.radiusControl
                color: letterTap.pressed ? lt.subtleFillPressed : lt.subtleFillHover
                opacity: letterTap.containsMouse || letter.current ? 1 : 0
                border.width: letter.current ? 2 : 0
                border.color: lt.focusOuter
            }
            PlasmaComponents3.Label {
                anchors.centerIn: parent
                text: letter.section
                font.pixelSize: lt.typeBodyLarge[0]
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: letterTap
                anchors.fill: parent
                hoverEnabled: true
                onClicked: apps.jump(letter.firstIndex)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        opacity: apps.folder ? 1 : 0
        scale: apps.folder ? 1 : 0.96
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFastMs; easing.type: lt.easingDecel } }
        Behavior on scale { NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel } }

        // Classic layout already has a Back button above.
        SectionHeader {
            text: apps.folder?.name ?? ""
            action: apps.showTitle ? i18n("Back") : ""
            icon: "arrow-left"
            onTriggered: apps.back()
        }

        GridView {
            id: folderGrid
            readonly property bool pinnable: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            leftMargin: menu.pad
            rightMargin: menu.pad
            clip: true
            keyNavigationEnabled: true
            currentIndex: -1
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor((width - leftMargin - rightMargin) / menu.cols)
            cellHeight: menu.tileH
            model: apps.folder?.members ?? null
            PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}
            Keys.onReturnPressed: start.launch(model, currentIndex)
            Keys.onEnterPressed: start.launch(model, currentIndex)
            Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
            delegate: AppTile { view: folderGrid }
        }
    }
}
