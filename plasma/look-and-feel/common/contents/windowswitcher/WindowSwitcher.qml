pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWin
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore

// Win11 Alt+Tab: centered acrylic sheet, live thumbnails with icon + title, accent focus ring.
KWin.TabBoxSwitcher {
    id: tabBox

    LumenTokens { id: lt }

    Instantiator {
        active: tabBox.visible
        delegate: PlasmaCore.Dialog {
            location: PlasmaCore.Types.Floating
            visible: true
            flags: Qt.ToolTip | Qt.X11BypassWindowManagerHint
            x: tabBox.screenGeometry.x + (tabBox.screenGeometry.width - mainItem.width) / 2
            y: tabBox.screenGeometry.y + (tabBox.screenGeometry.height - mainItem.height) / 2

            mainItem: FocusScope {
                id: sheet
                focus: true

                readonly property real aspect: tabBox.screenGeometry.width / Math.max(1, tabBox.screenGeometry.height)
                readonly property int thumbW: lt.sizeSwitcherThumbW
                readonly property int thumbH: Math.round(thumbW / aspect)
                readonly property int cellW: thumbW + 24
                readonly property int cellH: thumbH + 56
                readonly property int maxCols: Math.max(1, Math.floor(tabBox.screenGeometry.width * 0.85 / cellW))
                readonly property int cols: Math.max(1, Math.min(grid.count, maxCols))
                readonly property int rows: Math.max(1, Math.ceil(grid.count / cols))

                width: cols * cellW + 16
                height: Math.min(rows * cellH, tabBox.screenGeometry.height * 0.8) + 16

                GridView {
                    id: grid
                    anchors.fill: parent
                    anchors.margins: 8
                    focus: true
                    model: tabBox.model
                    currentIndex: tabBox.currentIndex
                    cellWidth: sheet.cellW
                    cellHeight: sheet.cellH
                    keyNavigationWraps: true
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    onCurrentIndexChanged: tabBox.currentIndex = currentIndex

                    delegate: MouseArea {
                        id: cell
                        required property int index
                        required property var model
                        readonly property bool current: GridView.isCurrentItem

                        width: grid.cellWidth
                        height: grid.cellHeight
                        hoverEnabled: true
                        focus: current
                        Accessible.name: model.caption
                        Accessible.role: Accessible.ListItem
                        onClicked: tabBox.model.activate(index)

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: lt.radiusOverlay
                            color: cell.containsMouse && !cell.current ? lt.subtleFillHover : "transparent"
                            border.width: cell.current ? 3 : 0
                            border.color: lt.textPrimary
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Kirigami.Icon {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: cell.model.icon
                                }
                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    text: cell.model.caption
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                    font.pixelSize: lt.typeCaption[0]
                                }
                                PlasmaComponents3.ToolButton {
                                    visible: cell.model.closeable && typeof tabBox.model.close !== "undefined" && (cell.containsMouse || cell.current)
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    icon.name: "window-close-symbolic"
                                    display: PlasmaComponents3.AbstractButton.IconOnly
                                    onClicked: tabBox.model.close(cell.index)
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                KWin.WindowThumbnail {
                                    anchors.fill: parent
                                    wId: cell.model.windowId
                                }
                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 48
                                    height: 48
                                    visible: cell.model.minimized
                                    source: cell.model.icon
                                }
                            }
                        }
                    }

                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Left) {
                            moveCurrentIndexLeft();
                        } else if (event.key === Qt.Key_Right) {
                            moveCurrentIndexRight();
                        } else if (event.key === Qt.Key_Up) {
                            moveCurrentIndexUp();
                        } else if (event.key === Qt.Key_Down) {
                            moveCurrentIndexDown();
                        } else {
                            return;
                        }
                        tabBox.currentIndex = currentIndex;
                        event.accepted = true;
                    }
                }

                PlasmaComponents3.Label {
                    anchors.centerIn: parent
                    visible: grid.count === 0
                    text: i18nd("kwin", "No open windows")
                }
            }
        }
    }
}
