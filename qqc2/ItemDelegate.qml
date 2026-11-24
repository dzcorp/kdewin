import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.desktop as Desktop
import org.kde.desktop.private as Private
import org.lumen.settings as Lumen

// Windows list item: subtle fill on hover and selection, accent pill on the selected row, text keeps its colour.
// Rows at rest create nothing; table cells keep the desktop style.
Desktop.ItemDelegate {
    id: control

    readonly property bool __table: TableView.view !== null

    // Selected text keeps the normal colour. Kirigami hands a colour override to children only from a theme that owns
    // its colours, so the row takes its parent's colour set instead of inheriting it, and reads the parent's text
    // colour (its own would loop).
    readonly property var __parentTheme: parent?.Kirigami.Theme ?? null
    Kirigami.Theme.inherit: __table || !__parentTheme
    Kirigami.Theme.colorSet: __parentTheme?.colorSet ?? Kirigami.Theme.View
    Kirigami.Theme.colorGroup: __parentTheme?.colorGroup ?? Kirigami.Theme.Active
    Binding {
        target: control.Kirigami.Theme
        property: "highlightedTextColor"
        value: control.__parentTheme?.textColor ?? control.palette.text
        when: !control.__table && control.__parentTheme
    }

    background: Item {
        Loader {
            anchors.fill: parent
            active: control.__table
            sourceComponent: Private.DefaultListItemBackground {
                control: control
            }
        }

        Loader {
            anchors.fill: parent
            active: !control.__table && (control.hovered || control.highlighted || control.down || control.visualFocus)
            sourceComponent: Item {
                LumenTokens {
                    id: lt
                }

                Rectangle {
                    anchors.fill: parent
                    radius: Lumen.Settings.controlRadius
                    color: control.down || (control.highlighted && control.hovered) ? lt.subtleFillPressed : control.hovered || control.highlighted ? lt.subtleFillHover : "transparent"
                    border.width: control.visualFocus ? lt.sizeFocusOuter : 0
                    border.color: lt.focusOuter
                }

                Rectangle {
                    visible: control.highlighted
                    anchors.verticalCenter: parent.verticalCenter
                    width: lt.sizeListPillW
                    height: control.down ? lt.sizeListPillH / 2 : lt.sizeListPillH
                    radius: lt.radiusListPill
                    color: lt.accentFill
                    Behavior on height {
                        NumberAnimation {
                            duration: lt.motionFastMs * Lumen.Settings.animationScale
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }
}
