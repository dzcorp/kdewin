pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.components as PlasmaComponents3
import org.lumen.notifications as Lumen

// Notification Center rows: app headers and their notifications.
// Rows animate their own height instead of using ListView transitions, which leave rows stranded, overlapping or
// half faded when changes interrupt each other. Removed rows stay listed while they animate out (the model keeps
// them as `leaving`), so the view lays out every row on every frame.
Item {
    id: view

    readonly property bool animated: root.expanded
    // Rows, including those animating out.
    readonly property int rows: list.count

    implicitHeight: list.contentHeight + list.bottomMargin

    Binding {
        target: center
        property: "linger"
        value: view.animated ? lt.motionFastMs + lt.motionSlowMs + 100 : 0
    }

    // Rows of `group` (all rows if empty) animate out, then `action` removes them.
    function leave(group: string, action) {
        const rows = Array.from(list.contentItem.children).filter(d => d.leave && !d.out && (group === "" || d.groupKey === group));
        if (!animated || rows.length === 0) {
            action();
            return;
        }
        rows.forEach((d, i) => d.leave(i === 0 ? action : null));
    }

    function clearGroup(group: string) {
        leave(group, () => {
            center.closeGroup(group);
            Lumen.NotificationStore.removeGroup(group);
        });
    }

    function positionViewAtBeginning() {
        list.positionViewAtBeginning();
    }

    // New arrivals wait while the pointer is over the list, so nothing shifts under it.
    HoverHandler { id: listHover }
    Binding {
        target: center
        property: "frozen"
        value: listHover.hovered && root.expanded
    }

    PlasmaComponents3.ScrollView {
        anchors.fill: parent
        PlasmaComponents3.ScrollBar.horizontal.policy: PlasmaComponents3.ScrollBar.AlwaysOff

        ListView {
            id: list
            model: center
            clip: true
            bottomMargin: 4
            boundsBehavior: Flickable.StopAtBounds

            // Roles are required properties: unlike `model`, they keep their values while a removed row animates out.
            delegate: Item {
                id: entry

                required property int index
                required property var kind
                required property var groupKey
                required property var groupHidden
                required property var groupExpanded
                required property var firstGroup
                required property var notificationId
                required property var key
                required property var type
                required property var summary
                required property var body
                required property var image
                required property var iconName
                required property var applicationName
                required property var applicationIconName
                required property var desktopEntry
                required property var created
                required property var updated
                required property var jobState
                required property var percentage
                required property var suspendable
                required property var killable
                required property var actionNames
                required property var actionLabels
                required property var hasDefaultAction
                required property var hasReplyAction
                required property var replyActionLabel
                required property var replyPlaceholderText
                required property var configurable
                required property var configureActionLabel
                required property var closable
                required property var resident
                required property var leaving
                required property var unread

                property real reveal: 1
                // Closed by the user: swipes out while still listed, then the close runs.
                property bool closing
                property var pendingAction: null
                readonly property bool out: leaving === true || closing

                function leave(action) {
                    if (!out) {
                        pendingAction = action ?? null;
                        closing = true;
                    }
                }
                function finished() {
                    const action = pendingAction;
                    pendingAction = null;
                    action?.();
                    if (closing) {
                        restoreCheck.start();
                    }
                }

                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                height: Math.round(body.implicitHeight * reveal)
                clip: reveal < 1
                enabled: !out

                onOutChanged: {
                    if (out) {
                        enter.stop();
                        exit.start();
                    } else {
                        exit.stop();
                        body.x = 0;
                        enter.start();
                    }
                }
                Component.onCompleted: {
                    if (out) {
                        reveal = 0;
                        body.opacity = 0;
                    }
                }
                ListView.onAdd: {
                    if (view.animated && !out) {
                        reveal = 0;
                        body.opacity = 0;
                        enter.start();
                    }
                }

                // Seen once at least half of it stayed on screen while the Notification Center is open.
                readonly property bool onScreen: view.animated && !out && reveal === 1
                    && Math.min(y + height, ListView.view.contentY + ListView.view.height) - Math.max(y, ListView.view.contentY) >= height / 2
                Timer {
                    interval: 400
                    running: entry.unread === true && entry.onScreen
                    onTriggered: center.markSeen(entry.index)
                }

                // The close did nothing (the row is neither leaving nor gone): bring it back.
                Timer {
                    id: restoreCheck
                    interval: 300
                    onTriggered: if (entry.closing && entry.leaving !== true) entry.closing = false
                }

                // Height changes resize the popup each frame, and Plasma may commit its size and position a frame
                // apart: an even curve keeps every step, and so any mismatch, a few pixels.
                ParallelAnimation {
                    id: enter
                    NumberAnimation { target: entry; property: "reveal"; to: 1; duration: lt.motionSlowMs; easing.type: Easing.InOutCubic }
                    NumberAnimation { target: body; property: "opacity"; to: 1; duration: lt.motionNormalMs; easing.type: lt.easingDecel }
                }
                SequentialAnimation {
                    id: exit
                    ParallelAnimation {
                        NumberAnimation { target: body; property: "x"; to: entry.closing ? entry.width : 0; duration: lt.motionFastMs; easing.type: lt.easingAccel }
                        NumberAnimation { target: body; property: "opacity"; to: 0; duration: lt.motionFastMs; easing.type: lt.easingAccel }
                    }
                    NumberAnimation { target: entry; property: "reveal"; to: 0; duration: lt.motionSlowMs; easing.type: Easing.InOutCubic }
                    ScriptAction { script: entry.finished() }
                }

                Loader {
                    id: body
                    width: parent.width
                    sourceComponent: entry.kind === Lumen.NotificationList.Header ? groupHeader : notification
                }

                Component {
                    id: groupHeader
                    GroupHeader {
                        model: entry
                        onClearRequested: view.clearGroup(entry.groupKey)
                    }
                }
                Component {
                    id: notification
                    NotificationItem {
                        model: entry
                        row: entry.index
                    }
                }
            }
        }
    }
}
