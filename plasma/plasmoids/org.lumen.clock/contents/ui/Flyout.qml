pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.plasma.clock
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.private.digitalclock as DigitalClock
import org.kde.plasma.workspace.calendar as PlasmaCalendar
import org.lumen.notifications as Lumen

// Win11 clock flyout: calendar, the selected day's events and world clocks; the Notification Center rides above it.
ColumnLayout {
    id: flyout

    readonly property real maxHeight: (root.availableScreenRect.height || Screen.height) * 0.85
    readonly property alias todayAuxText: month.todayAuxilliaryText

    Layout.minimumWidth: lt.sizeNotificationCenterW - 2 * lt.sizeFlyoutPad
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth: Layout.minimumWidth
    // Bound to the content: the popup must never keep a stretched, previously saved size.
    Layout.minimumHeight: Math.min(implicitHeight, maxHeight)
    Layout.preferredHeight: Math.min(implicitHeight, maxHeight)
    Layout.maximumHeight: Math.min(implicitHeight, maxHeight)
    spacing: 0

    Keys.onEscapePressed: root.expanded = false

    // Windows: the Notification Center is its own panel above the calendar, 12 px apart, reaching up to the screen
    // top while it lists anything. Both close once neither has focus (see root.hideOnWindowDeactivate).
    PlasmaCore.Dialog {
        id: centerPanel

        readonly property var popup: flyout.Window.window
        // AppletPopup is a C++ QQuickWindow and has no QML screen property. Plasma's available rect is
        // screen-relative; use the applet's screen origin so vertically offset monitors stay in bounds.
        readonly property int top: root.screenGeometry.y + root.availableScreenRect.y + 8
        readonly property int full: popup ? Math.max(0, popup.y - 12 - top) : 0
        readonly property int frame: margins.top + margins.bottom

        type: PlasmaCore.Dialog.Notification
        // Notification windows already stay above normal windows. WindowStaysOnTopHint turns this into a Dock.
        flags: Qt.WindowDoesNotAcceptFocus
        location: PlasmaCore.Types.Floating
        hideOnWindowDeactivate: false
        visible: root.expanded && (popup?.visible ?? false) && full > frame
        x: popup ? popup.x + popup.width - width : 0
        y: popup ? popup.y - 12 - height : 0
        onActiveChanged: {
            if (!active) {
                Qt.callLater(() => {
                    if (!centerPanel.active && !(centerPanel.popup?.active ?? false) && !Plasmoid.configuration.pin) {
                        root.expanded = false;
                    }
                });
            }
        }

        mainItem: NotificationPanel {
            id: panel
            width: lt.sizeNotificationCenterW - centerPanel.margins.left - centerPanel.margins.right
            height: Math.max(0, (rows > 0 ? centerPanel.full : Math.min(centerPanel.full, 232)) - centerPanel.frame)
        }
    }

    ColumnLayout {
        id: calendarBox
        Layout.fillWidth: true
        spacing: 0

        property bool collapsed: !Plasmoid.configuration.showCalendar
        readonly property bool showAgenda: !collapsed && Plasmoid.configuration.enabledCalendarPlugins.length > 0
        readonly property bool showClocks: !collapsed && root.zones.length > 1

        // Windows: long date and the collapse button in a 48 px row, divider under it.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            spacing: 4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: Qt.locale().toString(month.currentDate, month.currentDate.getFullYear() === root.now.getFullYear() ? "dddd, d MMMM" : "dddd, d MMMM yyyy")
                    elide: Text.ElideRight
                    font.pixelSize: lt.typeBody[0]
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: month.currentDateAuxilliaryText
                    elide: Text.ElideRight
                    color: lt.textSecondary
                    font.pixelSize: lt.typeCaption[0]
                }
            }
            PlasmaComponents3.ToolButton {
                icon.name: "window-pin"
                checkable: true
                checked: Plasmoid.configuration.pin
                display: PlasmaComponents3.AbstractButton.IconOnly
                text: i18n("Keep open")
                PlasmaComponents3.ToolTip.text: text
                PlasmaComponents3.ToolTip.visible: hovered
                onToggled: Plasmoid.configuration.pin = checked
            }
            PlasmaComponents3.ToolButton {
                icon.name: calendarBox.collapsed ? "arrow-up" : "arrow-down"
                display: PlasmaComponents3.AbstractButton.IconOnly
                text: calendarBox.collapsed ? i18n("Show calendar") : i18n("Hide calendar")
                PlasmaComponents3.ToolTip.text: text
                PlasmaComponents3.ToolTip.visible: hovered
                onClicked: calendarBox.collapsed = !calendarBox.collapsed
            }
        }

        PlasmaCalendar.EventPluginsManager {
            id: eventPlugins
            enabledPlugins: Plasmoid.configuration.enabledCalendarPlugins
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: -lt.sizeFlyoutPad
            Layout.rightMargin: -lt.sizeFlyoutPad
            Layout.preferredHeight: 1
            color: lt.divider
            visible: !calendarBox.collapsed
        }

        WinCalendar {
            id: month
            Layout.fillWidth: true
            Layout.leftMargin: 3 - lt.sizeFlyoutPad
            Layout.rightMargin: 3 - lt.sizeFlyoutPad
            Layout.preferredHeight: calendarBox.collapsed ? 0 : implicitHeight
            visible: Layout.preferredHeight > 0
            clip: true
            today: root.now
            showWeekNumbers: Plasmoid.configuration.showWeekNumbers
            firstDayOfWeek: Plasmoid.configuration.firstDayOfWeek < 0 ? Qt.locale().firstDayOfWeek : Plasmoid.configuration.firstDayOfWeek
            Behavior on Layout.preferredHeight { NumberAnimation { duration: lt.motionSlowMs; easing.type: Easing.InOutCubic } }
        }

        // The selected day's events from the enabled calendar add-ons.
        ColumnLayout {
            id: agenda
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 8
            Layout.bottomMargin: 8
            visible: calendarBox.showAgenda
            spacing: 4

            property var events: []

            function refresh() {
                events = month.daysModel.eventsForDate(month.currentDate);
            }
            // QDate reaches QML as UTC midnight; the month view's date is local.
            function sameDay(a: date, b: date): bool {
                return a.getUTCFullYear() === b.getFullYear() && a.getUTCMonth() === b.getMonth() && a.getUTCDate() === b.getDate();
            }

            Connections {
                target: month
                function onCurrentDateChanged() {
                    agenda.refresh();
                }
            }
            Connections {
                target: month.daysModel
                function onAgendaUpdated(updated: date) {
                    if (agenda.sameDay(updated, month.currentDate)) {
                        agenda.refresh();
                    }
                }
            }
            Component.onCompleted: refresh()

            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: i18n("Events")
                    color: lt.textSecondary
                    font.pixelSize: lt.typeCaption[0]
                    font.weight: Font.DemiBold
                }
                PlasmaComponents3.ToolButton {
                    visible: DigitalClock.ApplicationIntegration.calendarInstalled
                    icon.name: "list-add-symbolic"
                    text: i18n("Add event")
                    font.pixelSize: lt.typeCaption[0]
                    onClicked: DigitalClock.ApplicationIntegration.launchCalendar()
                }
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                visible: agenda.events.length === 0
                text: month.isToday(month.currentDate) ? i18n("No events for today") : i18n("No events for this day")
                color: lt.textTertiary
                font.pixelSize: lt.typeCaption[0]
            }

            ListView {
                id: eventList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 168)
                visible: count > 0
                clip: true
                interactive: contentHeight > height
                boundsBehavior: Flickable.StopAtBounds
                spacing: 4
                model: agenda.events
                delegate: EventRow {}
            }
        }

        // World clocks: tap one to show its time on the taskbar.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            Layout.bottomMargin: 8
            visible: calendarBox.showClocks
            spacing: 0

            PlasmaComponents3.Label {
                Layout.leftMargin: 8
                Layout.bottomMargin: 4
                text: i18n("Time zones")
                color: lt.textSecondary
                font.pixelSize: lt.typeCaption[0]
                font.weight: Font.DemiBold
            }
            Repeater {
                model: root.zones
                delegate: ZoneRow {}
            }
        }
    }

    // Windows calendar (title with ▲▼, 47 px columns, 42 px rows, today in an accent circle; the title zooms out to
    // years and decades) with the KDE calendar's week numbers, alternate-calendar labels, add-on event colours,
    // wheel and keyboard navigation and a way back to today.
    component WinCalendar: ColumnLayout {
        id: cal

        property date today
        property date currentDate: today
        property alias firstDayOfWeek: backend.firstDayOfWeek
        readonly property alias daysModel: backend.daysModel
        property string todayAuxilliaryText: ""
        property string currentDateAuxilliaryText: ""
        property bool showWeekNumbers: false
        // 0 days, 1 months, 2 years.
        property int level: 0
        readonly property int weekColumn: showWeekNumbers ? 28 : 0
        readonly property real cell: (width - weekColumn) / 7
        readonly property int decade: backend.year - backend.year % 10
        readonly property bool away: level !== 0 || backend.month !== today.getMonth() + 1 || backend.year !== today.getFullYear()
        readonly property int pageHeight: 16 + 26 + 10 + 6 * 42 + 2

        function sameDay(a: date, b: date): bool {
            return a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate();
        }
        function isToday(d: date): bool {
            return sameDay(d, today);
        }
        function resetToToday() {
            backend.resetToToday();
            currentDate = today;
            level = 0;
        }
        function step(forward: bool) {
            if (level === 0) {
                forward ? backend.nextMonth() : backend.previousMonth();
            } else if (level === 1) {
                forward ? backend.nextYear() : backend.previousYear();
            } else {
                forward ? backend.nextDecade() : backend.previousDecade();
            }
        }
        function select(d: date) {
            currentDate = d;
            if (d.getMonth() + 1 !== backend.month || d.getFullYear() !== backend.year) {
                backend.goToYearAndMonth(d.getFullYear(), d.getMonth() + 1);
            }
        }
        function moveDays(n: int) {
            select(new Date(currentDate.getFullYear(), currentDate.getMonth(), currentDate.getDate() + n));
        }
        function moveMonths(n: int) {
            const last = new Date(currentDate.getFullYear(), currentDate.getMonth() + n + 1, 0).getDate();
            select(new Date(currentDate.getFullYear(), currentDate.getMonth() + n, Math.min(currentDate.getDate(), last)));
        }

        spacing: 0
        activeFocusOnTab: true
        Keys.onPressed: event => {
            const keys = {
                [Qt.Key_Left]: () => moveDays(Qt.application.layoutDirection === Qt.RightToLeft ? 1 : -1),
                [Qt.Key_Right]: () => moveDays(Qt.application.layoutDirection === Qt.RightToLeft ? -1 : 1),
                [Qt.Key_Up]: () => moveDays(-7),
                [Qt.Key_Down]: () => moveDays(7),
                [Qt.Key_PageUp]: () => moveMonths(-1),
                [Qt.Key_PageDown]: () => moveMonths(1),
                [Qt.Key_Home]: () => resetToToday(),
            };
            if (keys[event.key]) {
                level = 0;
                keys[event.key]();
                event.accepted = true;
            }
        }

        PlasmaCalendar.Calendar {
            id: backend
            days: 7
            weeks: 6
            today: cal.today
            Component.onCompleted: daysModel.setPluginsManager(eventPlugins)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 9
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            Layout.preferredHeight: 40
            spacing: 0

            PlasmaComponents3.ToolButton {
                Layout.preferredHeight: 32
                Layout.preferredWidth: titleLabel.implicitWidth + leftPadding + rightPadding
                enabled: cal.level < 2
                leftPadding: 11
                rightPadding: 11
                Accessible.name: cal.level === 0 ? i18n("Show months") : i18n("Show years")
                onClicked: cal.level++
                contentItem: PlasmaComponents3.Label {
                    id: titleLabel
                    verticalAlignment: Text.AlignVCenter
                    text: cal.level === 0 ? Qt.locale().standaloneMonthName(backend.month - 1, Locale.LongFormat).replace(/^./, c => c.toUpperCase()) + " " + backend.year
                        : cal.level === 1 ? backend.year : cal.decade + " – " + (cal.decade + 9)
                    elide: Text.ElideRight
                    color: lt.textPrimary
                    font.pixelSize: lt.typeBodyStrong[0]
                    font.weight: lt.typeBodyStrong[2]
                }
            }
            Item { Layout.fillWidth: true }
            PlasmaComponents3.ToolButton {
                visible: cal.away
                Layout.preferredHeight: 32
                text: i18n("Today")
                font.pixelSize: lt.typeCaption[0]
                onClicked: cal.resetToToday()
            }
            Repeater {
                model: [{ up: true, text: i18n("Previous") }, { up: false, text: i18n("Next") }]
                delegate: PlasmaComponents3.ToolButton {
                    id: stepButton
                    required property var modelData
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 32
                    Accessible.name: modelData.text
                    onClicked: cal.step(!modelData.up)
                    contentItem: Item {
                        Shape {
                            anchors.centerIn: parent
                            width: 8
                            height: 6
                            rotation: stepButton.modelData.up ? 0 : 180
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                fillColor: lt.textSecondary
                                strokeColor: "transparent"
                                startX: 0; startY: 6
                                PathLine { x: 4; y: 0 }
                                PathLine { x: 8; y: 6 }
                                PathLine { x: 0; y: 6 }
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: page
            Layout.fillWidth: true
            Layout.preferredHeight: cal.pageHeight

            WheelHandler {
                property real delta: 0
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    delta += event.angleDelta.y;
                    while (Math.abs(delta) >= 120) {
                        cal.step(delta < 0);
                        delta -= Math.sign(delta) * 120;
                    }
                }
            }

            // Days.
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: cal.level === 0

                Row {
                    Layout.topMargin: 16
                    Layout.preferredHeight: 26
                    Item {
                        width: cal.weekColumn
                        height: 1
                    }
                    Repeater {
                        model: 7
                        delegate: PlasmaComponents3.Label {
                            required property int index
                            width: cal.cell
                            height: 26
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: Qt.locale().dayName((backend.firstDayOfWeek + index) % 7, Locale.ShortFormat).replace(/^./, c => c.toUpperCase())
                            font.pixelSize: lt.typeCaption[0]
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Row {
                    Layout.topMargin: 10
                    Layout.bottomMargin: 2

                    Column {
                        visible: cal.showWeekNumbers
                        Repeater {
                            model: cal.showWeekNumbers ? backend.weeksModel : []
                            delegate: PlasmaComponents3.Label {
                                required property var modelData
                                width: cal.weekColumn
                                height: 42
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                text: modelData
                                color: lt.textTertiary
                                font.pixelSize: lt.typeCaption[0]
                            }
                        }
                    }

                    Grid {
                        columns: 7
                        Repeater {
                            model: backend.daysModel
                            delegate: Item {
                                id: day
                                required property var model
                                readonly property date date: new Date(model.yearNumber, model.monthNumber - 1, model.dayNumber)
                                readonly property bool isToday: cal.sameDay(date, cal.today)
                                readonly property bool selected: cal.sameDay(date, cal.currentDate)
                                readonly property string sub: model.subDayLabel && model.subDayLabel !== String(model.dayNumber) ? model.subDayLabel : ""
                                width: cal.cell
                                height: 42
                                Accessible.role: Accessible.Button
                                Accessible.name: Qt.locale().toString(date, Locale.LongFormat)

                                onIsTodayChanged: if (isToday) cal.todayAuxilliaryText = Qt.binding(() => day.model.subLabel || "")
                                onSelectedChanged: if (selected) cal.currentDateAuxilliaryText = Qt.binding(() => day.model.subLabel || "")
                                Component.onCompleted: {
                                    if (isToday) {
                                        cal.todayAuxilliaryText = Qt.binding(() => day.model.subLabel || "");
                                    }
                                    if (selected) {
                                        cal.currentDateAuxilliaryText = Qt.binding(() => day.model.subLabel || "");
                                    }
                                }

                                HoverHandler { id: dayHover }
                                TapHandler {
                                    onTapped: {
                                        cal.forceActiveFocus();
                                        cal.select(day.date);
                                    }
                                }
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 40
                                    height: 40
                                    radius: 20
                                    color: day.isToday ? (dayHover.hovered ? lt.accentFillHover : lt.accentFill) : dayHover.hovered ? lt.subtleFillHover : "transparent"
                                    border.width: day.selected && !day.isToday ? 2 : 0
                                    border.color: lt.accentFill
                                }
                                PlasmaComponents3.Label {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: day.sub ? -5 : 0
                                    text: day.model.dayNumber
                                    color: day.isToday ? lt.textOnAccent : day.model.isCurrent ? lt.textPrimary : lt.textTertiary
                                    font.pixelSize: lt.typeBody[0]
                                }
                                PlasmaComponents3.Label {
                                    visible: day.sub !== ""
                                    anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter; verticalCenterOffset: 8 }
                                    text: day.sub
                                    color: day.isToday ? lt.textOnAccent : lt.textTertiary
                                    font.pixelSize: 9
                                }
                                Rectangle {
                                    visible: (day.model.eventCount ?? 0) > 0
                                    anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: day.sub ? 1 : 6 }
                                    width: 4
                                    height: 4
                                    radius: 2
                                    color: day.isToday ? lt.textOnAccent : day.model.eventColor || lt.accentFill
                                }
                            }
                        }
                    }
                }
            }

            // Months of the shown year, then the next year's first four (as Windows).
            Grid {
                anchors { fill: parent; topMargin: 16 }
                visible: cal.level === 1
                columns: 4
                Repeater {
                    model: 16
                    delegate: ZoomCell {
                        required property int index
                        readonly property int year: backend.year + Math.floor(index / 12)
                        readonly property int monthIndex: index % 12
                        text: Qt.locale().standaloneMonthName(monthIndex, Locale.ShortFormat).replace(/^./, c => c.toUpperCase())
                        current: year === cal.today.getFullYear() && monthIndex === cal.today.getMonth()
                        outside: index >= 12
                        onPicked: {
                            backend.goToYearAndMonth(year, monthIndex + 1);
                            cal.level = 0;
                        }
                    }
                }
            }

            // Years of the shown decade and the next six.
            Grid {
                anchors { fill: parent; topMargin: 16 }
                visible: cal.level === 2
                columns: 4
                Repeater {
                    model: 16
                    delegate: ZoomCell {
                        required property int index
                        readonly property int year: cal.decade + index
                        text: year
                        current: year === cal.today.getFullYear()
                        outside: index >= 10
                        onPicked: {
                            backend.goToYear(year);
                            cal.level = 1;
                        }
                    }
                }
            }
        }
    }

    component ZoomCell: Item {
        id: zoom
        property alias text: zoomLabel.text
        property bool current
        property bool outside
        signal picked()

        width: parent.width / 4
        height: (parent.height - 2) / 4
        Accessible.role: Accessible.Button
        Accessible.name: text

        HoverHandler { id: zoomHover }
        TapHandler { onTapped: zoom.picked() }
        Rectangle {
            anchors.centerIn: parent
            width: 56
            height: 56
            radius: 28
            color: zoom.current ? (zoomHover.hovered ? lt.accentFillHover : lt.accentFill) : zoomHover.hovered ? lt.subtleFillHover : "transparent"
        }
        PlasmaComponents3.Label {
            id: zoomLabel
            anchors.centerIn: parent
            color: zoom.current ? lt.textOnAccent : zoom.outside ? lt.textTertiary : lt.textPrimary
            font.pixelSize: lt.typeBody[0]
        }
    }

    component EventRow: RowLayout {
        id: event
        // Crashes if typed as the eventData gadget.
        required property var modelData
        readonly property var day: month.currentDate
        // Explicit all-day, a multi-day event spanning the whole day, or midnight-to-midnight.
        readonly property bool hasTime: {
            const s = modelData.startDateTime;
            const e = modelData.endDateTime;
            if (modelData.isAllDay || (s - day < 0 && e - day > 86400000)) {
                return false;
            }
            const midnight = d => d.getHours() === 0 && d.getMinutes() === 0;
            return !(midnight(s) && midnight(e) && s.getDate() === e.getDate());
        }
        width: ListView.view.width
        spacing: 8

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            radius: 1.5
            color: event.modelData.eventColor || lt.accentFill
        }
        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 64
            text: event.hasTime ? Qt.formatTime(event.modelData.startDateTime, Qt.locale().timeFormat(Locale.ShortFormat))
                + "\n" + Qt.formatTime(event.modelData.endDateTime, Qt.locale().timeFormat(Locale.ShortFormat)) : i18n("All day")
            color: lt.textSecondary
            font.pixelSize: lt.typeCaption[0]
            font.features: { "tnum": 1 }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: event.modelData.title
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: event.modelData.description
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                textFormat: Text.StyledText
                color: lt.textSecondary
                font.pixelSize: lt.typeCaption[0]
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }
    }

    component ZoneRow: Item {
        id: zoneRow
        required property string modelData
        readonly property bool current: modelData === root.zone

        Layout.fillWidth: true
        implicitHeight: 32

        Clock {
            id: zoneClock
            timeZone: zoneRow.modelData
        }
        HoverHandler { id: zoneHover }
        TapHandler { onTapped: Plasmoid.configuration.lastSelectedTimezone = zoneRow.modelData }

        Rectangle {
            anchors.fill: parent
            radius: lt.radiusControl
            color: lt.subtleFillHover
            opacity: zoneHover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
        }
        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            spacing: 8
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: root.zoneName(zoneClock) + root.formatOffset(zoneClock.dateTime)
                elide: Text.ElideRight
                font.weight: zoneRow.current ? Font.DemiBold : Font.Normal
            }
            PlasmaComponents3.Label {
                text: root.formatZoneTime(zoneClock.dateTime)
                font.weight: zoneRow.current ? Font.DemiBold : Font.Normal
                font.features: { "tnum": 1 }
            }
        }
    }

    Connections {
        target: root
        function onExpandedChanged() {
            if (root.expanded) {
                month.resetToToday();
                panel.positionViewAtBeginning();
            }
        }
    }
}
