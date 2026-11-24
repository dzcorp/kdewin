pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

// Win11 Start: search, pinned, recommended, all apps, account + power footer.
// Classic layout pages between Home and All apps; the single-page layout scrolls everything in one view.
FocusScope {
    id: menu

    readonly property bool searching: search.text !== ""
    property string page: "home"
    readonly property bool singlePage: Plasmoid.configuration.singlePage
    // Insets from the content edge; the popup adds lt.sizeFlyoutPad around it (Windows: 32 from the menu edge).
    readonly property int pad: 20
    readonly property int gridPad: 0
    // Search and account bands, a shade darker than the menu.
    readonly property color band: lt.dark ? "#14000000" : "#05000000"
    // Windows widens Start on screens 1600 px and wider: 8 pinned and 4 category columns.
    readonly property bool large: (Plasmoid.containment?.screenGeometry.width || Screen.width) >= lt.sizeStartLargeFrom
    readonly property int cols: large ? lt.sizeStartColsLarge : lt.sizeStartCols
    readonly property int tileH: lt.sizeStartTile
    readonly property int pinnedRows: (singlePage ? [2, 3, 1] : [3, 4, 2])[Plasmoid.configuration.startLayout] ?? 3
    readonly property int recommendedRows: [2, 1, 3][Plasmoid.configuration.startLayout] ?? 2
    // Windows: at most the screen minus the taskbar minus 61 px (12 below the menu, 49 above).
    readonly property real maxH: Math.min(large ? lt.sizeStartHLarge : lt.sizeStartH, (Plasmoid.containment?.availableScreenRect.height || Screen.desktopAvailableHeight) - 61) - 2 * lt.sizeFlyoutPad
    // Off while the popup resets on open, so pages don't animate into place.
    property bool animate: false
    // Keyboard-navigable views; arrows move between them in on-screen order.
    property var views: []
    signal reset()

    function register(view: Item) {
        views.push(view);
    }

    function unregister(view: Item) {
        views = views.filter(v => v !== view);
    }

    function step(from: Item, dir: int) {
        const top = v => v.contentItem.mapToItem(menu, 0, 0).y;
        const chain = views.filter(v => v && v.visible && v.enabled && v.count > 0).sort((a, b) => top(a) - top(b));
        const to = chain[chain.indexOf(from) + dir];
        if (!to) {
            if (dir < 0) {
                search.forceActiveFocus();
            }
            return;
        }
        to.currentIndex = dir > 0 ? 0 : (to.lastIndex ?? to.count - 1);
        to.forceActiveFocus();
    }

    // Scrolls the nearest interactive view so `item` is fully visible.
    function reveal(item: Item) {
        for (let p = item.parent; p; p = p.parent) {
            if (p instanceof Flickable && p.interactive) {
                const y = item.mapToItem(p.contentItem, 0, 0).y;
                if (y < p.contentY) {
                    p.contentY = y;
                } else if (y + item.height > p.contentY + p.height) {
                    p.contentY = y + item.height - p.height;
                }
                return;
            }
        }
    }

    function back(): bool {
        if (currentApps()?.back()) {
            return true;
        }
        if (searching) {
            search.clear();
            return true;
        }
        if (page !== "home") {
            page = "home";
            search.forceActiveFocus();
            return true;
        }
        return false;
    }

    function currentApps(): Item {
        if (searching) {
            return null;
        }
        return page === "all" ? allLoader.item?.apps ?? null : singlePage && page === "home" ? homeLoader.item : null;
    }

    Layout.minimumWidth: (large ? lt.sizeStartWLarge : lt.sizeStartW) - 2 * lt.sizeFlyoutPad
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth: Layout.minimumWidth
    Layout.minimumHeight: maxH
    Layout.preferredHeight: maxH
    Layout.maximumHeight: maxH

    Keys.onEscapePressed: {
        if (!back()) {
            start.expanded = false;
        }
    }
    // Typing anywhere searches.
    Keys.onPressed: event => {
        if (search.activeFocus || (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
            return;
        }
        if (event.key === Qt.Key_Backspace && searching) {
            search.text = search.text.slice(0, -1);
        } else if (event.text.length > 0 && (event.text.charCodeAt(0) > 32 || (event.text === " " && searching)) && event.text.charCodeAt(0) !== 127) {
            search.text += event.text;
        } else {
            return;
        }
        search.forceActiveFocus();
        event.accepted = true;
    }

    Connections {
        target: start
        function onExpandedChanged() {
            if (start.expanded) {
                menu.animate = false;
                search.clear();
                search.clicked = false;
                menu.page = "home";
                menu.reset();
                search.forceActiveFocus();
                Qt.callLater(() => menu.animate = true);
            }
        }
    }

    component Page: FocusScope {
        id: pageItem
        property bool active
        property int side: 0
        property bool used: false

        anchors.fill: parent
        enabled: active
        opacity: active ? 1 : 0
        visible: opacity > 0
        onActiveChanged: if (active) used = true
        Behavior on opacity {
            enabled: menu.animate
            NumberAnimation { duration: pageItem.active ? lt.motionNormalMs : lt.motionFastMs; easing.type: lt.easingDecel }
        }
        transform: Translate {
            x: pageItem.active ? 0 : pageItem.side * 48
            Behavior on x {
                enabled: menu.animate
                NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel }
            }
        }
    }

    Rectangle {
        x: -lt.sizeFlyoutPad
        y: -lt.sizeFlyoutPad
        width: menu.width + 2 * lt.sizeFlyoutPad
        height: lt.sizeFlyoutPad + search.y + search.height + 16
        topLeftRadius: lt.radiusOverlay
        topRightRadius: lt.radiusOverlay
        color: menu.band
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: lt.divider
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Keeps focus so typing (and input methods) land here, but looks at rest, without caret or accent,
        // until a search starts or the box is clicked.
        PlasmaComponents3.TextField {
            id: search
            property bool clicked: false
            readonly property bool engaged: menu.searching || clicked
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.leftMargin: menu.pad
            Layout.rightMargin: menu.pad
            Layout.bottomMargin: 17
            Layout.preferredHeight: lt.sizeControlH
            focus: true
            placeholderText: i18n("Search for apps, settings, and documents")
            leftPadding: 44
            rightPadding: 12
            font.pixelSize: lt.typeBody[0]
            topPadding: 0
            bottomPadding: 0
            onPressed: clicked = true
            onTextChanged: start.runnerModel.query = text
            background: Rectangle {
                radius: height / 2
                color: lt.controlFillInputActive
                border.width: 1
                border.color: search.engaged ? lt.accentFill : lt.controlStrokeSecondary
            }
            cursorDelegate: Rectangle {
                id: caret
                width: 1
                color: search.color
                visible: search.engaged && search.activeFocus && search.selectionStart === search.selectionEnd
                Connections {
                    target: search
                    function onCursorPositionChanged() {
                        caret.opacity = 1;
                        blink.restart();
                    }
                }
                Timer {
                    id: blink
                    running: caret.visible && Qt.styleHints.cursorFlashTime > 0
                    repeat: true
                    interval: Qt.styleHints.cursorFlashTime / 2
                    onTriggered: caret.opacity = 1 - caret.opacity
                    onRunningChanged: caret.opacity = 1
                }
            }
            Kirigami.Icon {
                roundToIconSize: false
                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                width: 16
                height: 16
                source: "search-symbolic"
                color: lt.textSecondary
                isMask: true
            }
            // While searching the caret stays here; arrows pick the result, as in Windows.
            Keys.onDownPressed: menu.searching ? searchLoader.item?.next() : menu.step(search, 1)
            Keys.onUpPressed: if (menu.searching) searchLoader.item?.previous()
            Keys.onReturnPressed: if (menu.searching) searchLoader.item?.accept()
            Keys.onEnterPressed: if (menu.searching) searchLoader.item?.accept()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Page {
                active: !menu.searching && menu.page === "home"
                side: -1
                Loader {
                    id: homeLoader
                    anchors.fill: parent
                    sourceComponent: menu.singlePage ? singleHome : classicHome
                }
            }

            Page {
                id: allPage
                active: !menu.searching && menu.page === "all"
                side: 1
                Loader {
                    id: allLoader
                    anchors.fill: parent
                    active: allPage.used && !menu.singlePage
                    sourceComponent: ColumnLayout {
                        readonly property alias apps: allApps
                        spacing: 8
                        SectionHeader {
                            text: i18n("All apps")
                            action: i18n("Back")
                            icon: "arrow-left"
                            onTriggered: menu.back()
                            ViewPicker { mode: allApps.mode }
                        }
                        AppsView {
                            id: allApps
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            Page {
                id: recommendedPage
                active: !menu.searching && menu.page === "recommended"
                side: 1
                Loader {
                    anchors.fill: parent
                    active: recommendedPage.used
                    sourceComponent: ColumnLayout {
                        spacing: 8
                        SectionHeader {
                            text: i18n("Recommended")
                            action: i18n("Back")
                            icon: "arrow-left"
                            onTriggered: menu.back()
                        }
                        ListView {
                            id: recentList
                            readonly property bool pinnable: false
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            leftMargin: menu.pad
                            rightMargin: menu.pad
                            clip: true
                            model: start.recentModel
                            keyNavigationEnabled: true
                            activeFocusOnTab: true
                            currentIndex: -1
                            highlightMoveDuration: 0
                            boundsBehavior: Flickable.StopAtBounds
                            onActiveFocusChanged: if (activeFocus && currentIndex < 0) currentIndex = 0
                            Component.onCompleted: menu.register(recentList)
                            Component.onDestruction: menu.unregister(recentList)
                            Keys.onReturnPressed: start.launch(model, currentIndex)
                            Keys.onEnterPressed: start.launch(model, currentIndex)
                            Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, false, 0, 0)
                            Keys.onUpPressed: event => {
                                if (currentIndex <= 0) {
                                    search.forceActiveFocus();
                                } else {
                                    event.accepted = false;
                                }
                            }
                            delegate: AppRow {
                                view: recentList
                                iconSize: 32
                            }
                            PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}
                        }
                    }
                }
            }

            Page {
                id: searchPage
                active: menu.searching
                Loader {
                    id: searchLoader
                    anchors.fill: parent
                    active: searchPage.used
                    sourceComponent: SearchPage { query: search.text }
                }
            }
        }

        Footer {
            Layout.fillWidth: true
        }
    }

    Component {
        id: classicHome
        ColumnLayout {
            spacing: 0
            PinnedSection {
                id: pinned
                Layout.fillWidth: true
                paged: true
                rows: menu.pinnedRows
                onAllApps: menu.page = "all"
                Connections {
                    target: menu
                    function onReset() { pinned.reset(); }
                }
            }
            RecommendedSection {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 42
                fill: true
                onMore: menu.page = "recommended"
            }
            Item {
                Layout.fillHeight: true
                visible: !Plasmoid.configuration.showRecommended
            }
        }
    }

    Component {
        id: singleHome
        AppsView {
            header: homeSections
            showTitle: true
        }
    }

    Component {
        id: homeSections
        ColumnLayout {
            spacing: 0
            PinnedSection {
                id: pinned
                Layout.fillWidth: true
                // Windows: the Pinned title sits 40 px under the search band.
                Layout.topMargin: 22
                paged: false
                rows: menu.pinnedRows
                Connections {
                    target: menu
                    function onReset() { pinned.reset(); }
                }
            }
            RecommendedSection {
                Layout.fillWidth: true
                Layout.topMargin: 24
                fill: false
                rows: menu.recommendedRows
                onMore: menu.page = "recommended"
            }
        }
    }
}
