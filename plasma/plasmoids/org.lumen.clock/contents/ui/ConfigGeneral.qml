import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    property alias cfg_displayMode: displayMode.currentIndex
    property alias cfg_dateStyle: dateStyle.currentIndex
    property alias cfg_showSeconds: showSeconds.checked
    property alias cfg_boldTime: boldTime.checked
    property alias cfg_hourFormat: hourFormat.currentIndex
    property alias cfg_dateFormat: dateFormat.text
    property alias cfg_timeSize: timeSize.value
    property alias cfg_dateSize: dateSize.value
    property alias cfg_timeAlign: timeAlign.currentIndex
    property alias cfg_dateAlign: dateAlign.currentIndex
    property bool cfg_showBell
    property bool cfg_bellOnlyUnread
    property alias cfg_bellCount: bellCount.checked
    property alias cfg_showCalendar: showCalendar.checked
    property alias cfg_displayTimezoneFormat: zoneFormat.currentIndex
    property bool cfg_showLocalTimezone
    property bool cfg_hideTimezone
    property bool cfg_showDate: true

    readonly property date sample: new Date()

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: displayMode
            Kirigami.FormData.label: i18n("Show:")
            model: [i18n("Time over date"), i18n("Time and date on one line"), i18n("Time only"), i18n("Date only")]
            onActivated: cfg_showDate = true
        }
        QQC2.ComboBox {
            id: hourFormat
            Kirigami.FormData.label: i18n("Time format:")
            model: [i18n("Use region settings"), i18n("12-hour"), i18n("24-hour")]
        }
        QQC2.CheckBox { id: showSeconds; text: i18n("Show seconds") }
        QQC2.CheckBox { id: boldTime; text: i18n("Bold time") }
        QQC2.ComboBox {
            id: dateStyle
            Kirigami.FormData.label: i18n("Date format:")
            model: [
                i18n("Region short (%1)", sample.toLocaleDateString(Qt.locale(), Locale.ShortFormat)),
                i18n("Weekday (%1)", Qt.locale().toString(sample, "ddd d MMM")),
                i18n("Region long (%1)", sample.toLocaleDateString(Qt.locale(), Locale.LongFormat)),
                i18n("ISO (%1)", Qt.locale().toString(sample, "yyyy-MM-dd")),
                i18n("Custom"),
            ]
        }
        QQC2.TextField {
            id: dateFormat
            visible: dateStyle.currentIndex === 4
            Kirigami.FormData.label: i18n("Custom format:")
            placeholderText: "ddd d MMM yyyy"
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Time size:")
            QQC2.Slider { id: timeSize; from: 70; to: 200; stepSize: 5 }
            QQC2.Label { text: timeSize.value + "%" }
        }
        QQC2.ComboBox {
            id: timeAlign
            Kirigami.FormData.label: i18n("Time alignment:")
            model: [i18n("Left"), i18n("Center"), i18n("Right")]
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Date size:")
            QQC2.Slider { id: dateSize; from: 70; to: 200; stepSize: 5 }
            QQC2.Label { text: dateSize.value + "%" }
        }
        QQC2.ComboBox {
            id: dateAlign
            Kirigami.FormData.label: i18n("Date alignment:")
            model: [i18n("Left"), i18n("Center"), i18n("Right")]
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.7
            text: i18n("Large sizes shrink to fit the taskbar; the taskbar itself never changes.")
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Notification bell:")
            model: [i18n("Always"), i18n("Only with new notifications"), i18n("Never")]
            currentIndex: !cfg_showBell ? 2 : cfg_bellOnlyUnread ? 1 : 0
            onActivated: index => {
                cfg_showBell = index !== 2;
                cfg_bellOnlyUnread = index === 1;
            }
        }
        QQC2.CheckBox {
            id: bellCount
            enabled: cfg_showBell
            text: i18n("Show number of new notifications")
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("Show time zone:")
            model: [i18n("Only when different from local"), i18n("Always"), i18n("Never")]
            currentIndex: cfg_hideTimezone ? 2 : cfg_showLocalTimezone ? 1 : 0
            onActivated: index => {
                cfg_hideTimezone = index === 2;
                cfg_showLocalTimezone = index === 1;
            }
        }
        QQC2.ComboBox {
            id: zoneFormat
            enabled: !cfg_hideTimezone
            Kirigami.FormData.label: i18n("Time zone format:")
            model: [i18n("Code"), i18n("City"), i18n("Offset from UTC")]
        }
        QQC2.CheckBox { id: showCalendar; Kirigami.FormData.label: i18n("Flyout:"); text: i18n("Show calendar") }
    }
}
