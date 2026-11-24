pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtCore
import org.kde.coreaddons as KCoreAddons
import org.kde.kcmutils as KCMUtils
import org.kde.kirigamiaddons.components as KirigamiComponents
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid
import org.kde.plasma.private.sessions
import org.kde.plasma.workspace.dbus as DBus

// Account, folder shortcuts and power, under an inset divider.
Item {
    id: footer

    property var firmwareCheck: null
    property var pending: null
    readonly property bool firmwareSetup: (firmwareCheck?.isFinished ?? false) && !firmwareCheck.isError && firmwareCheck.value === "yes"
    readonly property var places: ({
        settings: { text: i18n("Settings"), icon: "settings-configure-symbolic", url: "" },
        files: { text: i18n("File Manager"), icon: "system-file-manager-symbolic", url: StandardPaths.writableLocation(StandardPaths.HomeLocation) },
        documents: { text: i18n("Documents"), icon: "folder-documents-symbolic", url: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) },
        downloads: { text: i18n("Downloads"), icon: "folder-download-symbolic", url: StandardPaths.writableLocation(StandardPaths.DownloadLocation) },
        music: { text: i18n("Music"), icon: "folder-music-symbolic", url: StandardPaths.writableLocation(StandardPaths.MusicLocation) },
        pictures: { text: i18n("Pictures"), icon: "folder-pictures-symbolic", url: StandardPaths.writableLocation(StandardPaths.PicturesLocation) },
        videos: { text: i18n("Videos"), icon: "folder-videos-symbolic", url: StandardPaths.writableLocation(StandardPaths.MoviesLocation) },
        network: { text: i18n("Network"), icon: "network-workgroup-symbolic", url: "remote:/" },
    })

    function open(place) {
        start.expanded = false;
        if (place.url === "") {
            start.settingsApp.trigger(0, "", null);
        } else {
            Qt.openUrlExternally(place.url);
        }
    }

    function act(fn) {
        start.expanded = false;
        fn();
    }

    function rebootToFirmware() {
        start.expanded = false;
        pending = DBus.SystemBus.asyncCall({
            service: "org.freedesktop.login1",
            path: "/org/freedesktop/login1",
            iface: "org.freedesktop.login1.Manager",
            member: "SetRebootToFirmwareSetup",
            signature: "b",
            arguments: [new DBus.bool(true)],
        });
        pending.finished.connect(() => {
            if (!pending.isError) {
                session.requestReboot(SessionManagement.ConfirmationMode.Skip);
            }
        });
    }

    // With the popup's bottom padding: a 64 px band, as in Windows.
    implicitHeight: 64 - lt.sizeFlyoutPad

    KCoreAddons.KUser { id: user }
    SessionManagement { id: session }

    Component.onCompleted: firmwareCheck = DBus.SystemBus.asyncCall({
        service: "org.freedesktop.login1",
        path: "/org/freedesktop/login1",
        iface: "org.freedesktop.login1.Manager",
        member: "CanRebootToFirmwareSetup",
    })

    Rectangle {
        anchors { fill: parent; leftMargin: -lt.sizeFlyoutPad; rightMargin: -lt.sizeFlyoutPad; bottomMargin: -lt.sizeFlyoutPad }
        bottomLeftRadius: lt.radiusOverlay
        bottomRightRadius: lt.radiusOverlay
        color: menu.band
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: lt.divider
        }
    }

    RowLayout {
        // The popup's bottom margin is footer space too: the row centres between divider and popup edge.
        anchors { fill: parent; topMargin: 1; bottomMargin: -lt.sizeFlyoutPad; leftMargin: menu.pad + 24; rightMargin: menu.pad + 24 }
        spacing: 4

        PlasmaComponents3.ToolButton {
            id: account
            Layout.preferredHeight: 48
            Accessible.name: user.fullName || user.loginName
            contentItem: RowLayout {
                spacing: 12
                KirigamiComponents.Avatar {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    name: user.fullName || user.loginName
                    source: user.faceIconUrl
                    cache: false
                }
                PlasmaComponents3.Label {
                    text: user.fullName || user.loginName
                    font.pixelSize: lt.typeCaption[0]
                }
            }
            onClicked: accountMenu.open(0, 0)
            PlasmaExtras.Menu {
                id: accountMenu
                visualParent: account
                placement: PlasmaExtras.Menu.TopPosedLeftAlignedPopup
                PlasmaExtras.MenuItem {
                    text: i18n("Change account settings")
                    icon: "system-users"
                    onClicked: footer.act(() => KCMUtils.KCMLauncher.openSystemSettings("kcm_users"))
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Lock")
                    icon: "system-lock-screen"
                    visible: session.canLock
                    onClicked: footer.act(() => session.lock())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Switch user")
                    icon: "system-switch-user"
                    visible: session.canSwitchUser
                    onClicked: footer.act(() => session.switchUser())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Sign out")
                    icon: "system-log-out"
                    visible: session.canLogout
                    onClicked: footer.act(() => session.requestLogout())
                }
            }
        }

        Item { Layout.fillWidth: true }

        Repeater {
            model: Plasmoid.configuration.footerFolders.filter(id => id in footer.places)
            delegate: PlasmaComponents3.ToolButton {
                required property string modelData
                readonly property var place: footer.places[modelData]
                icon.name: place.icon
                display: PlasmaComponents3.AbstractButton.IconOnly
                text: place.text
                PlasmaComponents3.ToolTip.text: text
                PlasmaComponents3.ToolTip.visible: hovered
                onClicked: footer.open(place)
            }
        }

        PlasmaComponents3.ToolButton {
            id: power
            icon.name: "system-shutdown-symbolic"
            display: PlasmaComponents3.AbstractButton.IconOnly
            text: i18n("Power")
            PlasmaComponents3.ToolTip.text: text
            PlasmaComponents3.ToolTip.visible: hovered
            onClicked: powerMenu.open(0, 0)
            PlasmaExtras.Menu {
                id: powerMenu
                visualParent: power
                placement: PlasmaExtras.Menu.TopPosedRightAlignedPopup
                PlasmaExtras.MenuItem {
                    text: i18n("Lock")
                    icon: "system-lock-screen"
                    visible: session.canLock
                    onClicked: footer.act(() => session.lock())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Sleep")
                    icon: "system-suspend"
                    visible: session.canSuspend
                    onClicked: footer.act(() => session.suspend())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Hibernate")
                    icon: "system-suspend-hibernate"
                    visible: session.canHibernate
                    onClicked: footer.act(() => session.hibernate())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Shut down")
                    icon: "system-shutdown"
                    visible: session.canShutdown
                    onClicked: footer.act(() => session.requestShutdown())
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Restart")
                    icon: "system-reboot"
                    visible: session.canReboot
                    onClicked: footer.act(() => session.requestReboot())
                }
                PlasmaExtras.MenuItem {
                    separator: true
                    visible: footer.firmwareSetup && session.canReboot
                }
                PlasmaExtras.MenuItem {
                    text: i18n("Restart to UEFI Firmware Setup")
                    icon: "settings-configure"
                    visible: footer.firmwareSetup && session.canReboot
                    onClicked: footer.rebootToFirmware()
                }
            }
        }
    }
}
