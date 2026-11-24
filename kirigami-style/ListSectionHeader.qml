import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami.platform as Platform
import org.kde.kirigami.primitives as Primitives
import org.kde.kirigami.controls as KC

// Windows group header: semibold title without Kirigami's trailing rule; extra content stays at the trailing side.
QQC2.ItemDelegate {
    id: listSection

    @Deprecated { reason: "Use base type's AbstractButton::text property directly" }
    property alias label: listSection.text
    default property alias _contents: rowLayout.data

    hoverEnabled: false
    activeFocusOnTab: false
    icon {
        width: Platform.Units.iconSizes.smallMedium
        height: Platform.Units.iconSizes.smallMedium
    }
    background: Item {}
    topPadding: Platform.Units.largeSpacing + Platform.Units.smallSpacing
    Accessible.role: Accessible.Heading

    contentItem: RowLayout {
        id: rowLayout
        spacing: Platform.Units.largeSpacing

        Primitives.Icon {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: listSection.icon.width
            implicitHeight: listSection.icon.height
            color: listSection.icon.color
            source: listSection.icon.name.length > 0 ? listSection.icon.name : listSection.icon.source
            visible: valid
        }
        KC.Heading {
            Layout.maximumWidth: rowLayout.width
            Layout.alignment: Qt.AlignVCenter
            level: 5
            type: KC.Heading.Primary
            text: listSection.text
            elide: Text.ElideRight
            Accessible.ignored: true
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
