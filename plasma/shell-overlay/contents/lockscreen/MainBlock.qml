import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import org.kde.kscreenlocker as ScreenLocker
import org.kde.breeze.components

// Lumen: stock lock-screen block (same API and authenticator handling) with a Win11 password box:
// fixed-width field and the submit arrow inside it.
SessionManagementScreen {
    id: sessionManager

    property alias passwordInputVisible: passwordLayout.visible
    readonly property alias mainPasswordBox: passwordBox
    property bool lockScreenUiVisible: false
    property alias showPassword: passwordBox.showPassword
    property int visibleBoundary: mapFromItem(passwordLayout, 0, 0).y
    onHeightChanged: visibleBoundary = mapFromItem(passwordLayout, 0, 0).y + passwordLayout.height + Kirigami.Units.smallSpacing

    signal passwordResult(string password)

    onUserSelected: passwordBox.forceActiveFocus(Qt.TabFocusReason)

    function startLogin() {
        const password = passwordBox.text;
        submit.forceActiveFocus();
        passwordResult(password);
    }

    Item {
        id: passwordLayout
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 296
        Layout.preferredHeight: 36

        PlasmaExtras.PasswordField {
            id: passwordBox
            anchors.fill: parent
            rightPadding: submit.width + 8
            text: PasswordSync.password
            placeholderText: i18ndc("plasma_shell_org.kde.plasma.desktop", "@info:placeholder in text field", "Password")
            focus: true
            cursorVisible: visible
            onAccepted: {
                if (sessionManager.lockScreenUiVisible) {
                    sessionManager.startLogin();
                }
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Left && !text) {
                    sessionManager.userList.decrementCurrentIndex();
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Right && !text) {
                    sessionManager.userList.incrementCurrentIndex();
                    event.accepted = true;
                }
            }
            Connections {
                target: root
                function onClearPassword() {
                    passwordBox.forceActiveFocus();
                    passwordBox.text = "";
                    passwordBox.text = Qt.binding(() => PasswordSync.password);
                }
                function onNotificationRepeated() {
                    sessionManager.playHighlightAnimation();
                }
            }
        }

        Binding {
            target: PasswordSync
            property: "password"
            value: passwordBox.text
        }

        PlasmaComponents3.ToolButton {
            id: submit
            anchors { right: parent.right; rightMargin: 2; verticalCenter: parent.verticalCenter }
            width: 32
            height: 30
            visible: passwordBox.text.length > 0
            Accessible.name: i18ndc("plasma_shell_org.kde.plasma.desktop", "@action:button accessible only", "Unlock")
            icon.name: LayoutMirroring.enabled ? "go-previous" : "go-next"
            display: PlasmaComponents3.AbstractButton.IconOnly
            onClicked: sessionManager.startLogin()
            Keys.onEnterPressed: clicked()
            Keys.onReturnPressed: clicked()
        }
    }

    component FailableLabel: PlasmaComponents3.Label {
        id: label
        required property int kind
        required property string text0
        visible: authenticator.authenticatorTypes & kind
        text: text0
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
        RejectPasswordAnimation {
            id: reject
            target: label
            onFinished: restore.restart()
        }
        Connections {
            target: authenticator
            function onNoninteractiveError(kind, authenticator) {
                if (kind & label.kind) {
                    label.text = Qt.binding(() => authenticator.errorMessage);
                    reject.start();
                }
            }
        }
        Timer {
            id: restore
            interval: Kirigami.Units.humanMoment
            onTriggered: label.text = Qt.binding(() => label.text0)
        }
    }

    FailableLabel {
        kind: ScreenLocker.Authenticator.Fingerprint
        text0: i18ndc("plasma_shell_org.kde.plasma.desktop", "@info:usagetip", "(or scan your fingerprint on the reader)")
    }
    FailableLabel {
        kind: ScreenLocker.Authenticator.Smartcard
        text0: i18ndc("plasma_shell_org.kde.plasma.desktop", "@info:usagetip", "(or scan your smartcard)")
    }
}
