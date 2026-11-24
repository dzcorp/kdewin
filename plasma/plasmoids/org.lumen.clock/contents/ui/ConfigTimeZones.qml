import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.config as KConfig
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.digitalclock
import org.kde.plasma.workspace.timezoneselector as TimeZone

// World clocks: zones listed in the flyout and tooltip; the selected one drives the taskbar clock.
Kirigami.PageRow {
    id: row

    property string title
    property string cfg_lastSelectedTimezone
    property alias cfg_selectedTimeZones: zones.selectedTimeZones
    property alias cfg_wheelChangesTimezone: wheel.checked

    // The zone picker page needs its own Cancel/Add buttons in place of the dialog's OK/Apply.
    property Item dialogFooter
    property Item pickerFooter: QQC2.DialogButtonBox {
        QQC2.Button {
            text: i18n("Cancel")
            onClicked: {
                picker.selectedTimeZone = "";
                row.currentIndex = 0;
            }
        }
        QQC2.Button {
            text: i18n("Add Selected Time Zone")
            icon.name: "list-add"
            enabled: picker.selectedTimeZone
            onClicked: {
                zones.selectedTimeZones = [...zones.selectedTimeZones, picker.selectedTimeZone];
                picker.selectedTimeZone = "";
                row.currentIndex = 0;
            }
        }
    }

    defaultColumnWidth: width
    globalToolBar.style: Kirigami.ApplicationHeaderStyle.Auto

    Component.onCompleted: dialogFooter = applicationWindow().footer
    onCurrentIndexChanged: {
        applicationWindow().footer = currentIndex === 1 ? pickerFooter : dialogFooter;
        (currentIndex === 1 ? dialogFooter : pickerFooter).parent = null;
    }
    onVisibleChanged: {
        if (!visible && pickerFooter.parent) {
            applicationWindow().footer = dialogFooter;
            pickerFooter.parent = null;
        }
    }

    initialPage: KCMUtils.ScrollViewKCM {
        title: row.title
        actions: [
            Kirigami.Action {
                text: i18n("Add Time Zone…")
                icon.name: "list-add-symbolic"
                onTriggered: {
                    if (row.depth === 1) {
                        row.push(pickerPage);
                    } else {
                        row.currentIndex = 1;
                    }
                }
            }
        ]

        TimeZoneModel {
            id: zones
            onSelectedTimeZonesChanged: {
                if (selectedTimeZones.length === 0) {
                    zones.selectLocalTimeZone();
                }
            }
        }

        view: ListView {
            id: list
            clip: true
            activeFocusOnTab: true
            currentIndex: -1
            model: TimeZoneFilterProxy {
                sourceModel: zones
                onlyShowChecked: true
            }
            delegate: Kirigami.RadioSubtitleDelegate {
                id: zone
                required property var model
                readonly property bool current: row.cfg_lastSelectedTimezone === model.timeZoneId
                readonly property bool sameAsLocal: !model.isLocalTimeZone && model.city === zones.localTimeZoneCity()
                width: ListView.view.width
                font.bold: current
                text: model.city
                subtitle: list.count < 2 ? "" : current ? i18n("Clock is currently using this time zone")
                    : sameAsLocal ? i18n("Hidden while this is the local time zone's city") : ""
                checked: current
                onToggled: {
                    if (checked) {
                        row.cfg_lastSelectedTimezone = model.timeZoneId;
                    }
                }
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.TitleSubtitle {
                        Layout.fillWidth: true
                        opacity: zone.sameAsLocal ? 0.75 : 1
                        title: zone.text
                        subtitle: zone.subtitle
                        reserveSpaceForSubtitle: true
                    }
                    QQC2.Button {
                        visible: zone.model.isLocalTimeZone && KConfig.KAuthorized.authorizeControlModule("kcm_clock.desktop")
                        text: i18n("Switch Systemwide Time Zone…")
                        icon.name: "preferences-system-time"
                        font.bold: false
                        onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_clock")
                    }
                    QQC2.Button {
                        visible: !zone.model.isLocalTimeZone && list.count > 1
                        icon.name: "edit-delete-remove"
                        text: i18n("Remove this time zone")
                        display: QQC2.AbstractButton.IconOnly
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        onClicked: zone.model.checked = false
                    }
                }
            }
            section.property: "isLocalTimeZone"
            section.delegate: Kirigami.ListSectionHeader {
                required property string section
                width: list.width
                text: section === "true" ? i18n("Systemwide Time Zone") : i18n("Additional Time Zones")
            }
            Kirigami.PlaceholderMessage {
                visible: list.count === 1
                anchors {
                    top: parent.verticalCenter
                    left: parent.left
                    right: parent.right
                    margins: Kirigami.Units.gridUnit * 2
                }
                text: i18n("Add more time zones to see them in the clock's flyout and tooltip, or to show one of them on the taskbar")
            }
        }

        extraFooterTopPadding: true
        footer: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            QQC2.CheckBox {
                id: wheel
                Layout.fillWidth: true
                enabled: list.count > 1
                text: i18n("Switch displayed time zone by scrolling over the clock")
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.leftMargin: wheel.indicator.width + wheel.spacing
                enabled: wheel.enabled
                text: i18n("This does not change the systemwide time zone. When you travel, switch the systemwide time zone instead.")
                font: Kirigami.Theme.smallFont
                wrapMode: Text.Wrap
            }
        }
    }

    property Item pickerPage: Kirigami.Page {
        padding: 0
        title: i18n("Choose Time Zone")
        TimeZone.TimezoneSelector {
            id: picker
            anchors.fill: parent
        }
    }
}
