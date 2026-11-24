import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

// Hosts the full representation of the active applet (detail page in Quick Settings, or the icon's own popup).
FocusScope {
    id: page

    property Item applet
    property bool showBack: false
    readonly property Item fullRep: applet?.fullRepresentationItem ?? null
    property Item attached: null

    onFullRepChanged: attach()
    Component.onCompleted: attach()

    function attach() {
        if (attached && attached !== fullRep) {
            attached.visible = false;
            attached.parent = storage;
        }
        attached = fullRep;
        if (!fullRep) {
            return;
        }
        fullRep.anchors.centerIn = undefined;
        fullRep.anchors.fill = undefined;
        fullRep.parent = holder;
        fullRep.anchors.fill = holder;
        fullRep.visible = true;
        if (fullRep instanceof PlasmaComponents3.Page || fullRep instanceof PlasmaExtras.Representation) {
            if (fullRep.header instanceof PlasmaExtras.PlasmoidHeading) {
                fullRep.header.background.visible = false;
            }
            if (fullRep.footer instanceof PlasmaExtras.PlasmoidHeading) {
                fullRep.footer.background.visible = false;
            }
        }
        if (page.activeFocus) {
            fullRep.forceActiveFocus();
        }
    }

    onActiveFocusChanged: if (activeFocus && fullRep) fullRep.forceActiveFocus()

    Item {
        id: storage
        visible: false
    }

    RowLayout {
        id: header
        visible: page.showBack
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: visible ? lt.sizeControlH + 8 : 0
        spacing: 8

        PlasmaComponents3.ToolButton {
            icon.name: Application.layoutDirection === Qt.RightToLeft ? "go-next-symbolic" : "go-previous-symbolic"
            display: PlasmaComponents3.AbstractButton.IconOnly
            text: i18n("Back")
            onClicked: trayState.back()
            KeyNavigation.down: page.fullRep
        }
        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: page.applet?.Plasmoid.title ?? ""
            elide: Text.ElideRight
            font.pixelSize: lt.typeBodyStrong[0]
            font.weight: lt.typeBodyStrong[2]
        }
    }

    Item {
        id: holder
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Back || (event.key === Qt.Key_Left && event.modifiers & Qt.AltModifier)) {
            trayState.back();
            event.accepted = true;
        }
    }
}
