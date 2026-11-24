pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

// Search: filter chips, results on the left, the selected result's details and actions on the right.
FocusScope {
    id: page

    required property string query
    property int filter: 0
    readonly property var filters: [
        { text: i18n("All"), runners: [] },
        { text: i18n("Apps"), runners: ["krunner_services"] },
        { text: i18n("Documents"), runners: ["baloosearch", "krunner_recentdocuments", "krunner_placesrunner", "locations"] },
        { text: i18n("Settings"), runners: ["krunner_systemsettings"] },
        { text: i18n("Web"), runners: ["krunner_webshortcuts", "krunner_bookmarksrunner", "browserhistory", "browsertabs"] },
    ]

    function next() {
        results.incrementCurrentIndex();
        results.positionViewAtIndex(results.currentIndex, ListView.Contain);
    }
    function previous() {
        results.decrementCurrentIndex();
        results.positionViewAtIndex(results.currentIndex, ListView.Contain);
    }
    function accept() {
        start.launch(results.model, results.currentIndex);
    }

    onQueryChanged: {
        settle.restart();
        results.currentIndex = 0;
        if (query === "") {
            filter = 0;
        }
    }
    onFilterChanged: {
        runnerModel.runners = filters[filter].runners;
        // The same query is ignored, and queries within 10 ms coalesce: clear first, search again shortly.
        runnerModel.query = "";
        settle.restart();
        requery.restart();
    }

    // Inside a Timer, `start` would name Timer.start().
    readonly property var runnerModel: start.runnerModel

    Timer {
        id: requery
        interval: 30
        onTriggered: page.runnerModel.query = page.query
    }

    // Holds "No results" back while runners start up for a new query.
    Timer {
        id: settle
        interval: 250
    }

    component Chip: T.AbstractButton {
        id: chip
        required property var modelData
        required property int index
        property bool selected

        implicitWidth: implicitContentWidth + 28
        implicitHeight: 30
        text: modelData.text
        activeFocusOnTab: true
        Accessible.role: Accessible.RadioButton
        Accessible.checked: selected

        background: Rectangle {
            radius: height / 2
            color: chip.selected ? (chip.down ? lt.accentFillPressed : chip.hovered ? lt.accentFillHover : lt.accentFill)
                                 : (chip.down ? lt.controlFill : chip.hovered ? lt.controlFillHover : lt.controlFill)
            border.width: chip.visualFocus ? 2 : chip.selected ? 0 : 1
            border.color: chip.visualFocus ? lt.focusOuter : lt.controlStroke
        }
        contentItem: PlasmaComponents3.Label {
            text: chip.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: chip.selected ? lt.textOnAccent : lt.textPrimary
            font.pixelSize: lt.typeCaption[0]
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: menu.pad
            Layout.rightMargin: menu.pad
            spacing: 8

            Repeater {
                model: page.filters
                delegate: Chip {
                    selected: page.filter === index
                    onClicked: page.filter = index
                }
            }
            Item { Layout.fillWidth: true }
            PlasmaComponents3.ToolButton {
                icon.name: "configure-symbolic"
                display: PlasmaComponents3.AbstractButton.IconOnly
                text: i18n("Configure Search…")
                PlasmaComponents3.ToolTip.text: text
                PlasmaComponents3.ToolTip.visible: hovered
                onClicked: {
                    start.expanded = false;
                    KCMUtils.KCMLauncher.openSystemSettings("kcm_plasmasearch");
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.bottomMargin: 12
            spacing: 12

            ListView {
                id: results
                readonly property bool pinnable: true
                readonly property bool showCurrent: true

                Layout.fillWidth: true
                Layout.fillHeight: true
                leftMargin: menu.pad
                model: start.runnerModel.count > 0 ? start.runnerModel.modelForRow(0) : null
                clip: true
                keyNavigationEnabled: true
                activeFocusOnTab: true
                currentIndex: 0
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds
                Accessible.name: i18n("Search results")
                onCountChanged: if (currentIndex < 0 && count > 0) currentIndex = 0
                section.property: "group"
                section.delegate: PlasmaComponents3.Label {
                    required property string section
                    width: ListView.view.width - menu.pad
                    height: 32
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                    text: section
                    font.weight: Font.DemiBold
                    color: lt.textSecondary
                }
                Keys.onReturnPressed: page.accept()
                Keys.onEnterPressed: page.accept()
                Keys.onMenuPressed: currentItem && start.openMenu(currentItem, model, currentIndex, true, 0, 0)
                Keys.onUpPressed: event => {
                    if (currentIndex <= 0) {
                        search.forceActiveFocus();
                    } else {
                        event.accepted = false;
                    }
                }
                delegate: AppRow {
                    view: results
                    iconSize: 32
                }
                PlasmaComponents3.ScrollBar.vertical: PlasmaComponents3.ScrollBar {}

                PlasmaExtras.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    visible: results.count === 0 && !start.runnerModel.querying && !settle.running
                    iconName: "edit-none"
                    text: i18n("No results for “%1”", page.query)
                }
            }

            Rectangle {
                id: pane
                readonly property var match: results.currentItem?.model ?? null
                readonly property var actions: {
                    start.favorites.count;
                    return match ? [{ text: i18n("Open"), icon: "document-open-symbolic", actionId: "_lumen_open" }].concat(start.actionsFor(match, match.favoriteId)) : [];
                }

                Layout.preferredWidth: Math.round(page.width * 0.4)
                Layout.fillHeight: true
                Layout.rightMargin: menu.pad
                visible: match !== null
                radius: lt.radiusOverlay
                color: lt.cardBg
                border.color: lt.cardStroke

                ColumnLayout {
                    anchors { fill: parent; margins: 12 }
                    spacing: 4

                    Kirigami.Icon {
                        roundToIconSize: false
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 16
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                        source: pane.match?.decoration ?? ""
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        horizontalAlignment: Text.AlignHCenter
                        text: pane.match?.display ?? ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        font.pixelSize: lt.typeBodyStrong[0]
                        font.weight: lt.typeBodyStrong[2]
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: pane.match?.group ?? ""
                        elide: Text.ElideRight
                        color: lt.textSecondary
                        font.pixelSize: lt.typeCaption[0]
                    }
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: text !== "" && text !== pane.match?.display
                        horizontalAlignment: Text.AlignHCenter
                        text: pane.match?.description ?? ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                        color: lt.textSecondary
                        font.pixelSize: lt.typeCaption[0]
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        Layout.bottomMargin: 4
                        implicitHeight: 1
                        color: lt.divider
                    }
                    ListView {
                        id: actionList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: pane.actions
                        delegate: Item {
                            id: action
                            required property var modelData
                            readonly property bool separator: modelData.type === "separator"
                            width: ListView.view.width
                            height: separator ? 9 : 32

                            Rectangle {
                                visible: action.separator
                                anchors.centerIn: parent
                                width: parent.width
                                height: 1
                                color: lt.divider
                            }
                            Rectangle {
                                visible: !action.separator
                                anchors.fill: parent
                                radius: lt.radiusControl
                                color: actionMouse.pressed ? lt.subtleFillPressed : lt.subtleFillHover
                                opacity: actionMouse.containsMouse ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
                            }
                            RowLayout {
                                visible: !action.separator
                                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                spacing: 10
                                Kirigami.Icon {
                                    roundToIconSize: false
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: action.modelData.icon ?? ""
                                }
                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    text: action.modelData.text ?? ""
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                    font.pixelSize: lt.typeCaption[0]
                                }
                            }
                            MouseArea {
                                id: actionMouse
                                anchors.fill: parent
                                enabled: !action.separator && !(action.modelData.disabled ?? false)
                                hoverEnabled: true
                                onClicked: {
                                    if (action.modelData.actionId === "_lumen_open") {
                                        page.accept();
                                    } else {
                                        start.triggerAction(results.model, results.currentIndex, action.modelData.actionId, action.modelData.actionArgument);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
