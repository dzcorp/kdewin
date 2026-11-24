pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.kwindowsystem
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.clock
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.private.digitalclock as DigitalClock
import org.lumen.notifications as Lumen

PlasmoidItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property date now: clock.dateTime
    // Notification times switch from time to date once a minute at most.
    readonly property int minute: Math.floor(now.getTime() / 60000)
    readonly property bool dnd: !isNaN(notificationSettings.notificationsInhibitedUntil.getTime())
        && root.now.getTime() < notificationSettings.notificationsInhibitedUntil.getTime()
    // Counted from arrival until the notification was on screen in the Notification Center.
    readonly property int unread: center.unread

    readonly property string timeFormat: {
        let f = Qt.locale().timeFormat(Locale.ShortFormat);
        const mode = Plasmoid.configuration.hourFormat;
        if (mode === 1) {
            f = f.replace(/H/g, "h");
            if (!/a/i.test(f)) {
                f += " AP";
            }
        } else if (mode === 2) {
            f = f.replace(/h/g, "H").replace(/\s*a+p*/gi, "").trim();
        }
        if (Plasmoid.configuration.showSeconds && f.indexOf("s") < 0) {
            f = f.replace(/(m+)/, "$1:ss");
        }
        return f;
    }
    readonly property string dateText: {
        const custom = Plasmoid.configuration.dateFormat;
        switch (Plasmoid.configuration.dateStyle) {
        case 1: return Qt.locale().toString(now, "ddd d MMM");
        case 2: return now.toLocaleDateString(Qt.locale(), Locale.LongFormat);
        case 3: return Qt.locale().toString(now, "yyyy-MM-dd");
        case 4: return custom !== "" ? Qt.locale().toString(now, custom) : now.toLocaleDateString(Qt.locale(), Locale.ShortFormat);
        default: return now.toLocaleDateString(Qt.locale(), Locale.ShortFormat);
        }
    }
    // World clocks: an explicit entry for the system zone is hidden while at home.
    readonly property var zones: DigitalClock.TimeZoneUtils.sortedTimeZones(Plasmoid.configuration.selectedTimeZones
        .filter(z => z === "Local" || z !== systemClock.timeZone))
    readonly property string zone: {
        const all = Plasmoid.configuration.selectedTimeZones;
        const last = Plasmoid.configuration.lastSelectedTimezone;
        return all.indexOf(last) >= 0 ? last : (all[0] ?? "Local");
    }
    readonly property string zoneLabel: !Plasmoid.configuration.hideTimezone && (Plasmoid.configuration.showLocalTimezone || !clock.isSystemTimeZone) ? zoneName(clock) : ""
    readonly property int mode: Plasmoid.configuration.showDate ? Plasmoid.configuration.displayMode
        : (Plasmoid.configuration.displayMode === 3 ? 3 : 2)
    readonly property real timePx: lt.typeCaption[0] * Plasmoid.configuration.timeSize / 100 * (mode === 2 ? 1.15 : 1)
    readonly property real datePx: lt.typeCaption[0] * Plasmoid.configuration.dateSize / 100 * (mode === 3 ? 1.15 : 1)

    function zoneName(c: Clock): string {
        switch (Plasmoid.configuration.displayTimezoneFormat) {
        case 1: return DigitalClock.TimeZonesI18n.i18nCity(c.timeZone);
        case 2: return c.timeZoneOffset;
        default: return c.timeZoneCode;
        }
    }
    function formatZoneTime(t: date): string {
        const s = Qt.locale().toString(t, root.timeFormat);
        return t.getDay() === root.now.getDay() ? s : s + " " + Qt.locale().toString(t, "ddd");
    }
    function formatOffset(t: date): string {
        const minutes = Math.round((t - root.now) / 60000);
        if (minutes === 0) {
            return "";
        }
        const h = Math.floor(Math.abs(minutes) / 60);
        const m = Math.abs(minutes) % 60;
        if (m !== 0) {
            return minutes > 0 ? i18nc("offset from current time in hours and minutes", " • %1:%2 later", h, String(m).padStart(2, "0"))
                : i18nc("offset from current time in hours and minutes", " • %1:%2 earlier", h, String(m).padStart(2, "0"));
        }
        return minutes > 0 ? i18ncp("offset from current time", " • %1 hour later", " • %1 hours later", h)
            : i18ncp("offset from current time", " • %1 hour earlier", " • %1 hours earlier", h);
    }
    function switchZone(step: int) {
        const all = Plasmoid.configuration.selectedTimeZones;
        if (all.length > 1) {
            const i = Math.max(0, all.indexOf(root.zone));
            Plasmoid.configuration.lastSelectedTimezone = all[((i + step) % all.length + all.length) % all.length];
        }
    }

    function hAlign(setting: int): int {
        return setting === 0 ? Qt.AlignLeft : setting === 1 ? Qt.AlignHCenter : Qt.AlignRight;
    }

    preferredRepresentation: Plasmoid.formFactor === PlasmaCore.Types.Horizontal || Plasmoid.formFactor === PlasmaCore.Types.Vertical ? compactRepresentation : fullRepresentation
    // The pointer is over the Notification Center panel: a click there must not close this popup.
    property bool centerHovered: false
    hideOnWindowDeactivate: !Plasmoid.configuration.pin && !centerHovered
    toolTipItem: Loader {
        Layout.minimumWidth: item?.implicitWidth ?? 0
        Layout.maximumWidth: item?.implicitWidth ?? 0
        Layout.minimumHeight: item?.implicitHeight ?? 0
        Layout.maximumHeight: item?.implicitHeight ?? 0
        source: Qt.resolvedUrl("ClockTooltip.qml")
    }
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.status: expanded ? PlasmaCore.Types.RequiresAttentionStatus : PlasmaCore.Types.ActiveStatus

    onExpandedChanged: {
        if (root.expanded) {
            hidePopups();
        } else {
            center.collapseAll();
        }
    }

    // Windows: opening the Notification Center takes the toasts off screen; they stay listed, actions included.
    function hidePopups() {
        const N = NotificationManager.Notifications;
        for (let i = history.count - 1; i >= 0; --i) {
            const idx = history.index(i, 0);
            if (!history.data(idx, N.ExpiredRole) && !history.data(idx, N.DismissedRole)
                && history.data(idx, N.TypeRole) === N.NotificationType && history.data(idx, N.UrgencyRole) !== N.CriticalUrgency) {
                history.setData(idx, true, N.DismissedRole);
            }
        }
    }

    LumenTokens { id: lt }

    Clock {
        id: clock
        timeZone: root.zone
        trackSeconds: Plasmoid.configuration.showSeconds
    }
    Clock { id: systemClock }

    NotificationManager.Settings { id: notificationSettings }

    NotificationManager.Notifications {
        id: history
        showExpired: true
        showDismissed: true
        showJobs: notificationSettings.jobsInNotifications
        sortMode: NotificationManager.Notifications.SortByDate
        groupMode: NotificationManager.Notifications.GroupDisabled
        ignoreBlacklistDuringInhibition: true
        blacklistedDesktopEntries: notificationSettings.historyBlacklistedApplications
        blacklistedNotifyRcNames: notificationSettings.historyBlacklistedServices
        urgencies: NotificationManager.Notifications.CriticalUrgency | NotificationManager.Notifications.NormalUrgency
            | (notificationSettings.lowPriorityHistory ? NotificationManager.Notifications.LowUrgency : 0)
    }

    // Grouped rows of the Notification Center: live notifications and those of earlier sessions.
    Lumen.NotificationList {
        id: center
        live: history
        earlier: Lumen.NotificationStore
    }

    // Everything the history shows is also kept on disk, so a restart or re-login doesn't lose it.
    NotificationManager.Notifications {
        id: recorder
        showExpired: true
        showDismissed: true
        showJobs: false
        blacklistedDesktopEntries: history.blacklistedDesktopEntries
        blacklistedNotifyRcNames: history.blacklistedNotifyRcNames
        urgencies: history.urgencies
    }
    Instantiator {
        model: recorder
        delegate: QtObject {
            required property var model
            readonly property string summary: model.summary ?? ""
            readonly property string body: model.body ?? ""
            function keep() {
                Lumen.NotificationStore.record({
                    notificationId: model.notificationId,
                    created: model.created,
                    summary: summary,
                    body: body,
                    applicationName: model.applicationName ?? "",
                    applicationIconName: model.applicationIconName ?? "",
                    iconName: model.iconName ?? "",
                    desktopEntry: model.desktopEntry ?? "",
                });
            }
            Component.onCompleted: keep()
            onSummaryChanged: keep()
            onBodyChanged: keep()
        }
    }

    function clearAll() {
        center.closeAll();
        history.clear(NotificationManager.Notifications.ClearExpired);
        Lumen.NotificationStore.clear();
    }

    function toggleDnd() {
        if (dnd || notificationSettings.notificationsInhibitedByApplication) {
            notificationSettings.notificationsInhibitedUntil = undefined;
            notificationSettings.revokeApplicationInhibitions();
        } else {
            const until = new Date();
            until.setFullYear(until.getFullYear() + 1);
            notificationSettings.notificationsInhibitedUntil = until;
        }
        notificationSettings.save();
    }

    compactRepresentation: MouseArea {
        id: compact

        // Windows ends the taskbar with a "show desktop" strip: the plate stops 12 px before the panel's end. Only
        // when the clock comes last (its distance to the end is just the panel's margin).
        property int endGap: 12
        readonly property int peek: Math.max(0, 12 - endGap)
        readonly property bool overPeek: containsMouse && (root.vertical ? mouseY >= height - peek : mouseX >= width - peek)
        function measureEnd() {
            const w = Window.window;
            if (w) {
                const p = mapToItem(null, width, height);
                endGap = Math.round(root.vertical ? w.height - p.y : w.width - p.x);
            }
        }
        onWidthChanged: Qt.callLater(measureEnd)
        onHeightChanged: Qt.callLater(measureEnd)
        Component.onCompleted: Qt.callLater(measureEnd)
        Connections {
            target: compact.Window.window
            function onWidthChanged() { Qt.callLater(compact.measureEnd); }
            function onHeightChanged() { Qt.callLater(compact.measureEnd); }
        }

        Layout.minimumWidth: root.vertical ? 0 : row.implicitWidth + 16 + peek
        Layout.preferredWidth: Layout.minimumWidth
        Layout.minimumHeight: root.vertical ? column.implicitHeight + 16 + peek : 0
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        property int wheelDelta: 0
        onWheel: wheel => {
            if (!Plasmoid.configuration.wheelChangesTimezone || Plasmoid.configuration.selectedTimeZones.length < 2) {
                wheel.accepted = false;
                return;
            }
            wheelDelta += wheel.angleDelta.y || wheel.angleDelta.x;
            const steps = Math.trunc(wheelDelta / 120);
            if (steps !== 0) {
                wheelDelta -= steps * 120;
                root.switchZone(-steps);
            }
        }
        onClicked: mouse => {
            if (overPeek) {
                KWindowSystem.showingDesktop = !KWindowSystem.showingDesktop;
            } else if (mouse.button === Qt.MiddleButton) {
                root.toggleDnd();
            } else {
                root.expanded = !root.expanded;
            }
        }

        Item {
            id: plateArea
            width: parent.width - (root.vertical ? 0 : compact.peek)
            height: parent.height - (root.vertical ? compact.peek : 0)
        }

        Rectangle {
            anchors.centerIn: plateArea
            width: plateArea.width
            height: Math.min(plateArea.height, lt.sizeTaskbarPlate)
            radius: lt.radiusTaskbarButton
            color: compact.pressed ? lt.controlFillPressed : lt.controlFillHover
            border.color: lt.controlStroke
            opacity: (compact.containsMouse && !compact.overPeek) || root.expanded ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
        }

        // Text never changes the panel: both lines shrink together to fit its thickness.
        readonly property real lineHeight: 1.3
        readonly property real wanted: root.vertical ? 0 : (root.mode === 0 ? (root.timePx + root.datePx) : Math.max(root.mode === 3 ? 0 : root.timePx, root.mode === 2 ? 0 : root.datePx)) * lineHeight
        // Vertical panels: the wider line shrinks to the panel's width instead.
        readonly property real fit: root.vertical ? Math.min(1, (width - 8) / Math.max(1, timeWidth.advanceWidth, dateWidth.advanceWidth))
            : wanted > 0 ? Math.min(1, (height - 4) / wanted) : 1
        TextMetrics {
            id: timeWidth
            text: timeLabel.text
            font.family: lt.fontUiSmall
            font.pixelSize: root.timePx
        }
        TextMetrics {
            id: dateWidth
            text: dateLabel.visible ? dateLabel.text : ""
            font.family: lt.fontUiSmall
            font.pixelSize: root.datePx
        }

        RowLayout {
            id: row
            anchors.centerIn: plateArea
            // DirectWrite puts the baseline 12 px into a 16 px Caption line, Qt 13 (rounded-up ascent).
            anchors.verticalCenterOffset: root.vertical ? 0 : -1
            spacing: 10

            GridLayout {
                id: column
                rowSpacing: 0
                columnSpacing: 6
                flow: root.mode === 1 && !root.vertical ? GridLayout.LeftToRight : GridLayout.TopToBottom
                PlasmaComponents3.Label {
                    id: timeLabel
                    Layout.alignment: (root.vertical ? Qt.AlignHCenter : root.hAlign(Plasmoid.configuration.timeAlign)) | Qt.AlignVCenter
                    visible: root.mode !== 3
                    text: Qt.locale().toString(root.now, root.timeFormat) + (root.mode === 2 && root.zoneLabel ? " " + root.zoneLabel : "")
                    font.family: lt.fontUiSmall
                    font.pixelSize: Math.max(6, root.timePx * compact.fit)
                    font.weight: Plasmoid.configuration.boldTime ? Font.DemiBold : Font.Normal
                }
                PlasmaComponents3.Label {
                    id: dateLabel
                    Layout.alignment: (root.vertical ? Qt.AlignHCenter : root.hAlign(Plasmoid.configuration.dateAlign)) | Qt.AlignVCenter
                    visible: root.mode !== 2
                    text: root.zoneLabel ? root.dateText + " (" + root.zoneLabel + ")" : root.dateText
                    opacity: root.mode === 1 ? 0.8 : 1
                    font.family: lt.fontUiSmall
                    font.pixelSize: Math.max(6, root.datePx * compact.fit)
                }
            }

            Item {
                readonly property bool count: Plasmoid.configuration.bellCount
                visible: Plasmoid.configuration.showBell && (!Plasmoid.configuration.bellOnlyUnread || root.unread > 0 || root.dnd)
                Layout.preferredWidth: lt.sizeTrayIcon
                Layout.preferredHeight: lt.sizeTrayIcon

                Kirigami.Icon {
                    roundToIconSize: false
                    anchors.fill: parent
                    source: root.dnd ? "notifications-disabled-symbolic" : "notifications-symbolic"
                    visible: root.dnd || root.unread === 0 || !parent.count
                }
                Rectangle {
                    visible: !root.dnd && root.unread > 0 && !parent.count
                    anchors { right: parent.right; top: parent.top }
                    width: 6
                    height: 6
                    radius: 3
                    color: lt.accentFill
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: !root.dnd && root.unread > 0 && parent.count
                    width: Math.max(height, badge.implicitWidth + 8)
                    height: 16
                    radius: 8
                    color: lt.accentFill
                    PlasmaComponents3.Label {
                        id: badge
                        anchors.centerIn: parent
                        text: root.unread > 99 ? "99+" : root.unread
                        color: lt.textOnAccent
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    fullRepresentation: Flyout {}

    Component.onCompleted: DigitalClock.ClipboardMenu.setupMenu(copyAction)
    Connections {
        target: Plasmoid
        function onContextualActionsAboutToShow() {
            DigitalClock.ClipboardMenu.secondsIncluded = Plasmoid.configuration.showSeconds;
            DigitalClock.ClipboardMenu.timezone = clock.timeZone;
        }
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            id: copyAction
            text: i18n("Copy to Clipboard")
            icon.name: "edit-copy-symbolic"
        },
        PlasmaCore.Action {
            text: i18n("Do not disturb")
            icon.name: "notifications-disabled-symbolic"
            checkable: true
            checked: root.dnd
            onTriggered: root.toggleDnd()
        },
        PlasmaCore.Action {
            text: i18n("Clear all notifications")
            icon.name: "edit-clear-history"
            enabled: history.count > 0 || Lumen.NotificationStore.count > 0
            onTriggered: root.clearAll()
        },
        PlasmaCore.Action {
            text: i18n("Adjust date and time…")
            icon.name: "clock"
            onTriggered: KCMUtils.KCMLauncher.openSystemSettings("kcm_clock")
        },
        PlasmaCore.Action {
            text: i18n("Notification settings…")
            icon.name: "configure-symbolic"
            onTriggered: KCMUtils.KCMLauncher.openSystemSettings("kcm_notifications")
        }
    ]
}
