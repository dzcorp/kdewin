import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// Wrapper libplasma puts around the compact representation of every applet hosted in this containment.
PlasmaCore.ToolTipArea {
    id: appletRoot
    objectName: "org.kde.desktop-CompactApplet"
    anchors.fill: parent

    property Item fullRepresentation
    property Item compactRepresentation
    property PlasmoidItem plasmoidItem

    mainText: plasmoidItem?.toolTipMainText ?? ""
    subText: plasmoidItem?.toolTipSubText ?? ""
    textFormat: plasmoidItem?.toolTipTextFormat ?? Text.AutoText
    mainItem: plasmoidItem?.toolTipItem ?? null
    location: Plasmoid.location
    active: plasmoidItem ? !plasmoidItem.expanded : false

    Layout.minimumWidth: Plasmoid.formFactor === PlasmaCore.Types.Vertical ? 0 : height
    Layout.minimumHeight: Plasmoid.formFactor === PlasmaCore.Types.Vertical ? width : 0

    Connections {
        target: Plasmoid
        function onContextualActionsAboutToShow() {
            appletRoot.hideImmediately();
        }
    }

    function adopt() {
        if (compactRepresentation) {
            compactRepresentation.parent = appletRoot;
            compactRepresentation.anchors.fill = appletRoot;
            compactRepresentation.visible = true;
        }
        appletRoot.visible = true;
    }
    onCompactRepresentationChanged: adopt()
    Component.onCompleted: adopt()
}
