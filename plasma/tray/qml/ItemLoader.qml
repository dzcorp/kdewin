pragma ComponentBehavior: Bound
import QtQuick

Loader {
    id: loader

    required property int index
    required property int effectiveStatus
    required property var model
    property bool inOverflow: false
    readonly property string itemId: model.itemId ?? ""

    z: x + 1

    readonly property url __url: {
        if (model.itemType === "Plasmoid" && model.hasApplet) {
            return Qt.resolvedUrl("PlasmoidTrayItem.qml");
        } else if (model.itemType === "StatusNotifier") {
            return Qt.resolvedUrl("SniTrayItem.qml");
        } else if (model.itemType === "BackgroundApp") {
            return Qt.resolvedUrl("BackgroundAppTrayItem.qml");
        }
        return "";
    }

    on__UrlChanged: {
        setSource(__url, {
            index: Qt.binding(() => loader.index),
            status: Qt.binding(() => loader.model.status),
            effectiveStatus: Qt.binding(() => loader.effectiveStatus),
            model: Qt.binding(() => loader.model),
            inOverflow: Qt.binding(() => loader.inOverflow),
        });
    }
}
