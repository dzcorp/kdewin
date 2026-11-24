pragma ComponentBehavior: Bound
import QtQuick
import org.kde.plasma.configuration
import org.kde.plasma.plasmoid
import org.kde.plasma.workspace.calendar as PlasmaCalendar

ConfigModel {
    id: configModel

    ConfigCategory {
        name: i18n("Appearance")
        icon: "preferences-desktop-color"
        source: "ConfigGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Calendar")
        icon: "office-calendar"
        source: "ConfigCalendar.qml"
    }
    ConfigCategory {
        name: i18n("Time Zones")
        icon: "preferences-system-time"
        source: "ConfigTimeZones.qml"
    }

    // Each enabled calendar add-on brings its own settings page.
    readonly property PlasmaCalendar.EventPluginsManager plugins: PlasmaCalendar.EventPluginsManager {
        Component.onCompleted: populateEnabledPluginsList(Plasmoid.configuration.enabledCalendarPlugins)
    }
    readonly property Instantiator pluginPages: Instantiator {
        model: configModel.plugins.model
        delegate: ConfigCategory {
            required property string display
            required property string decoration
            required property string configUi
            required property string configModule
            required property string configComponent
            required property string pluginId

            name: display
            icon: decoration
            source: configUi
            configUiModule: configModule
            configUiComponent: configComponent
            visible: Plasmoid.configuration.enabledCalendarPlugins.indexOf(pluginId) > -1
        }
        onObjectAdded: (index, object) => configModel.appendCategory(object)
        onObjectRemoved: (index, object) => configModel.removeCategory(object)
    }
}
