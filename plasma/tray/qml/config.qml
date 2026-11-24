import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Entries")
        icon: "preferences-desktop-notification"
        source: "ConfigEntries.qml"
    }
}
