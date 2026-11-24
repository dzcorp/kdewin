import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

FocusScope {
    id: content

    readonly property bool appletMode: trayState.showingApplet
    readonly property int qsWidth: lt.sizeQuicksettingsW
    // Quick Settings and the pages it opens share one size, so switching reads as one window.
    readonly property int qsHeight: Math.max(quick.implicitHeight, Kirigami.Units.gridUnit * 24)
    readonly property int appletWidth: trayState.fromQuickSettings ? qsWidth : Kirigami.Units.gridUnit * 24
    readonly property int appletHeight: trayState.fromQuickSettings ? qsHeight : Kirigami.Units.gridUnit * 24

    Layout.minimumWidth: appletMode ? appletWidth : qsWidth
    Layout.maximumWidth: Layout.minimumWidth
    Layout.preferredWidth: Layout.minimumWidth
    Layout.minimumHeight: appletMode ? appletHeight : qsHeight
    Layout.maximumHeight: Layout.minimumHeight
    Layout.preferredHeight: Layout.minimumHeight

    onActiveFocusChanged: if (activeFocus) (appletMode ? page : quick).forceActiveFocus()
    onAppletModeChanged: if (activeFocus) (appletMode ? page : quick).forceActiveFocus()

    QuickSettings {
        id: quick
        width: content.qsWidth
        height: content.qsHeight
        visible: opacity > 0
        opacity: content.appletMode ? 0 : 1
        x: content.appletMode ? -24 : 0
        Behavior on opacity { NumberAnimation { duration: lt.motionFastMs } }
        Behavior on x { NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel } }
    }

    AppletPage {
        id: page
        width: parent.width
        height: parent.height
        applet: trayState.activeApplet
        showBack: trayState.fromQuickSettings
        visible: opacity > 0
        opacity: content.appletMode ? 1 : 0
        x: content.appletMode ? 0 : 24
        Behavior on opacity { NumberAnimation { duration: lt.motionFastMs } }
        Behavior on x { NumberAnimation { duration: lt.motionNormalMs; easing.type: lt.easingDecel } }
    }
}
