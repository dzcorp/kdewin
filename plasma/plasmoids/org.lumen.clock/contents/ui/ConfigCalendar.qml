import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.workspace.calendar as PlasmaCalendar

// Week layout and the calendar add-ons (holidays, astronomical events, alternate calendars, PIM events).
KCMUtils.ScrollViewKCM {
    id: page

    signal configurationChanged

    property alias cfg_showWeekNumbers: weekNumbers.checked
    property int cfg_firstDayOfWeek
    property bool unsavedChanges: false

    // Add-ons are applied outside the cfg_ mechanism, so the dialog tracks them through these.
    function saveConfig() {
        Plasmoid.configuration.enabledCalendarPlugins = plugins.enabledPlugins;
        Plasmoid.configuration.writeConfig();
        unsavedChanges = false;
    }
    function checkUnsavedChanges() {
        const saved = Plasmoid.configuration.enabledCalendarPlugins;
        const now = plugins.enabledPlugins;
        unsavedChanges = saved.length !== now.length || !saved.every(p => now.includes(p));
    }

    extraFooterTopPadding: true

    header: Kirigami.FormLayout {
        PlasmaCalendar.EventPluginsManager {
            id: plugins
            Component.onCompleted: populateEnabledPluginsList(Plasmoid.configuration.enabledCalendarPlugins)
        }
        QQC2.CheckBox {
            id: weekNumbers
            Kirigami.FormData.label: i18n("General:")
            text: i18n("Show week numbers")
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18n("First day of week:")
            textRole: "text"
            model: [-1, 0, 1, 5, 6].map(day => ({
                day,
                text: day === -1 ? i18n("Use region defaults") : Qt.locale().dayName(day),
            }))
            currentIndex: model.findIndex(item => item.day === page.cfg_firstDayOfWeek)
            onActivated: index => page.cfg_firstDayOfWeek = model[index].day
        }
        Item {
            Kirigami.FormData.isSection: true
        }
    }

    view: ListView {
        id: pluginList
        activeFocusOnTab: true
        model: plugins.model
        header: Kirigami.InlineViewHeader {
            width: pluginList.width
            text: i18n("Available Add-Ons")
        }
        headerPositioning: ListView.OverlayHeader
        delegate: QQC2.CheckDelegate {
            id: addOn
            required property var model
            width: pluginList.width
            checked: model.checked
            text: model.display
            icon.source: model.decoration
            onClicked: {
                model.checked = checked;
                page.checkUnsavedChanges();
            }
            contentItem: Kirigami.IconTitleSubtitle {
                width: addOn.availableWidth
                icon: icon.fromControlsIcon(addOn.icon)
                title: addOn.text
                subtitle: addOn.model.toolTip
                selected: addOn.highlighted || addOn.pressed
            }
        }
    }
}
