pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrolsaddons as KQuickControlsAddons
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.components as PlasmaComponents3
import org.lumen.notifications as Lumen

// One row of the Notification Center. Earlier-session rows have no sender left, so clicking opens their app.
Item {
    id: note

    // The list row: its roles, plus leave().
    required property var model
    required property int row

    readonly property bool live: model?.kind === Lumen.NotificationList.Live
    readonly property bool isJob: live && model?.type === NotificationManager.Notifications.JobType
    readonly property bool clickable: live ? (model?.hasDefaultAction ?? false) : (model?.desktopEntry ?? "") !== ""
    readonly property bool hovered: hover.hovered
    readonly property bool configurable: live && (model?.configurable ?? false)
    readonly property bool closable: !live || (model?.closable ?? false)
    readonly property int buttonCount: (configurable ? 1 : 0) + (closable ? 1 : 0)
    // Set on first hover and kept, so the buttons are built once and can fade.
    property bool armed
    property bool replying
    readonly property date when: model?.updated instanceof Date && !isNaN(model?.updated.getTime()) ? model?.updated : model?.created ?? new Date(NaN)

    function sourceIndex() {
        return center.sourceIndex(row);
    }
    function forget() {
        if (live && model?.created) {
            Lumen.NotificationStore.remove(Lumen.NotificationStore.keyOf(model?.notificationId, model?.created));
        }
    }
    function close() {
        if (row >= 0) {
            model.leave(closeNow);
        }
    }
    function closeNow() {
        if (row < 0) {
            return;
        }
        if (live) {
            forget();
            history.close(sourceIndex());
        } else {
            Lumen.NotificationStore.remove(model?.key);
        }
    }
    function activate() {
        if (row < 0) {
            return;
        }
        if (!live) {
            Lumen.NotificationStore.launch(sourceIndex().row);
            return;
        }
        if (!model?.resident) {
            forget();
        }
        history.invokeDefaultAction(sourceIndex(), model?.resident ? NotificationManager.Notifications.None : NotificationManager.Notifications.Close);
    }
    function invoke(action: string) {
        history.invokeAction(sourceIndex(), action, model?.resident ? NotificationManager.Notifications.None : NotificationManager.Notifications.Close);
    }

    // A card per notification, 4 px apart: time on top, then picture and text (Windows Notification Center).
    implicitHeight: content.implicitHeight + 24 + 4

    HoverHandler {
        id: hover
        onHoveredChanged: if (hovered) note.armed = true
    }
    TapHandler {
        id: tap
        enabled: note.clickable
        onTapped: note.activate()
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: parent.height - 4
        radius: lt.radiusControl
        color: tap.pressed ? lt.controlFillPressed : note.hovered ? lt.controlFill : lt.cardBgSecondary
        border.color: lt.cardStroke
        Behavior on color { ColorAnimation { duration: lt.motionFasterMs } }
    }

    ColumnLayout {
        id: content
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
        spacing: 12

        // Time, and the buttons on hover. Cards slide up after a close, so the next close button lands under the pointer.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 16

            PlasmaComponents3.Label {
                id: time
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                // Windows: the time of day; a date only past a day.
                text: isNaN(note.when.getTime()) ? "" : root.minute * 60000 - note.when < 86400000 ? note.when.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                    : Qt.locale().toString(note.when, Locale.ShortFormat)
                color: lt.textSecondary
                font.pixelSize: lt.typeCaption[0]
            }
            Loader {
                anchors.right: parent.right
                anchors.rightMargin: -8
                anchors.verticalCenter: parent.verticalCenter
                active: note.armed
                sourceComponent: Row {
                    property bool shown
                    spacing: 2
                    opacity: note.hovered && shown ? 1 : 0
                    enabled: note.hovered
                    Behavior on opacity { NumberAnimation { duration: lt.motionFasterMs } }
                    Component.onCompleted: shown = true

                    PlasmaComponents3.ToolButton {
                        visible: note.configurable
                        width: 24
                        height: 24
                        icon.name: "configure-symbolic"
                        display: PlasmaComponents3.AbstractButton.IconOnly
                        text: note.model?.configureActionLabel || i18n("Configure")
                        PlasmaComponents3.ToolTip.text: text
                        PlasmaComponents3.ToolTip.visible: hovered
                        onClicked: history.configure(note.sourceIndex())
                    }
                    PlasmaComponents3.ToolButton {
                        visible: note.closable
                        width: 24
                        height: 24
                        icon.name: "window-close-symbolic"
                        display: PlasmaComponents3.AbstractButton.IconOnly
                        text: i18n("Close")
                        onClicked: note.close()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Item {
                visible: picture.visible || icon.visible
                Layout.preferredWidth: picture.visible ? 48 : 32
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignTop

                Loader {
                    id: picture
                    anchors.fill: parent
                    active: note.live
                    visible: !(picture.item?.null ?? true)
                    sourceComponent: KQuickControlsAddons.QImageItem {
                        image: note.model?.image
                        fillMode: KQuickControlsAddons.QImageItem.PreserveAspectFit
                    }
                }
                Kirigami.Icon {
                    id: icon
                    anchors.fill: parent
                    roundToIconSize: false
                    // Without an app icon the group header already shows this one.
                    visible: !picture.visible && !note.isJob && (note.model?.iconName ?? "") !== "" && !!note.model?.applicationIconName
                        && note.model?.iconName !== note.model?.applicationIconName
                    source: note.model?.iconName ?? ""
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: note.model?.summary ?? ""
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    font.pixelSize: lt.typeBody[0]
                }
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: note.model?.body ?? ""
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    textFormat: Text.StyledText
                    color: lt.textSecondary
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }

                // Controls most rows never show are created on demand: Plasma buttons cost milliseconds each.
                Loader {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    active: note.isJob
                    visible: active
                    sourceComponent: RowLayout {
                        spacing: 4
                        PlasmaComponents3.ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: note.model?.percentage ?? 0
                            indeterminate: note.model?.jobState === NotificationManager.Notifications.JobStateRunning && (note.model?.percentage ?? 0) <= 0
                        }
                        PlasmaComponents3.ToolButton {
                            readonly property bool paused: note.model?.jobState === NotificationManager.Notifications.JobStateSuspended
                            visible: note.model?.suspendable ?? false
                            icon.name: paused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic"
                            display: PlasmaComponents3.AbstractButton.IconOnly
                            text: paused ? i18n("Resume") : i18n("Pause")
                            onClicked: paused ? history.resumeJob(note.sourceIndex()) : history.suspendJob(note.sourceIndex())
                        }
                        PlasmaComponents3.ToolButton {
                            visible: note.model?.killable ?? false
                            icon.name: "media-playback-stop-symbolic"
                            display: PlasmaComponents3.AbstractButton.IconOnly
                            text: i18n("Cancel")
                            onClicked: history.killJob(note.sourceIndex())
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    active: note.live && ((note.model?.actionNames ?? []).some(a => a !== "default") || (note.model?.hasReplyAction ?? false))
                    visible: active
                    sourceComponent: Flow {
                        spacing: 6

                        Repeater {
                            model: note.model?.actionNames ?? []
                            delegate: PlasmaComponents3.Button {
                                required property int index
                                required property string modelData
                                visible: modelData !== "default"
                                text: (note.model?.actionLabels ?? [])[index] ?? ""
                                onClicked: note.invoke(modelData)
                            }
                        }
                        PlasmaComponents3.Button {
                            visible: (note.model?.hasReplyAction ?? false) && !note.replying
                            text: note.model?.replyActionLabel || i18n("Reply")
                            icon.name: "mail-reply-sender"
                            onClicked: note.replying = true
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    active: note.replying
                    visible: active
                    onLoaded: item.forceActiveFocus()
                    sourceComponent: PlasmaComponents3.TextField {
                        placeholderText: note.model?.replyPlaceholderText || i18n("Type a reply…")
                        onAccepted: {
                            if (text !== "") {
                                history.reply(note.sourceIndex(), text, NotificationManager.Notifications.Close);
                            }
                        }
                    }
                }
            }
        }
    }
}
