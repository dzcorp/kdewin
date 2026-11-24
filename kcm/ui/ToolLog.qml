import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

LumenCard {
    visible: kcm.log !== ""
    Layout.topMargin: Kirigami.Units.largeSpacing

    RowLayout {
        Layout.margins: Kirigami.Units.largeSpacing
        QQC2.BusyIndicator {
            visible: kcm.busy
            running: visible
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
        }
        QQC2.Label {
            Layout.fillWidth: true
            text: kcm.busy ? i18n("Running…") : i18n("Output")
            font.bold: true
        }
    }
    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 12
        Layout.margins: Kirigami.Units.largeSpacing
        QQC2.TextArea {
            readOnly: true
            wrapMode: TextEdit.Wrap
            font.family: "monospace"
            text: kcm.log
            onTextChanged: cursorPosition = length
        }
    }
}
