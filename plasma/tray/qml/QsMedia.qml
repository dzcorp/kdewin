import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.private.mpris as Mpris

// Media card for the current MPRIS player.
Rectangle {
    id: card

    Mpris.Mpris2Model { id: mpris }

    readonly property var player: mpris.currentPlayer
    readonly property bool playing: player?.playbackStatus === Mpris.PlaybackStatus.Playing
    readonly property bool available: player !== null && player.playbackStatus !== Mpris.PlaybackStatus.Stopped

    implicitHeight: 88
    radius: lt.radiusOverlay
    color: lt.cardBg
    border.width: 1
    border.color: lt.cardStroke

    RowLayout {
        anchors { fill: parent; margins: 12 }
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: lt.radiusControl
            color: lt.controlFill
            clip: true
            Image {
                id: art
                anchors.fill: parent
                source: card.player?.artUrl ?? ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(128, 128)
            }
            Kirigami.Icon {
                roundToIconSize: false
                anchors.centerIn: parent
                width: 32
                height: 32
                visible: art.status !== Image.Ready
                source: card.player?.iconName || "media-default-album"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: card.player?.track || card.player?.identity || ""
                elide: Text.ElideRight
                font.pixelSize: lt.typeBodyStrong[0]
                font.weight: lt.typeBodyStrong[2]
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: card.player?.artist ?? ""
                visible: text !== ""
                elide: Text.ElideRight
                color: lt.textSecondary
                font.pixelSize: lt.typeCaption[0]
            }
            RowLayout {
                spacing: 4
                PlasmaComponents3.ToolButton {
                    icon.name: "media-skip-backward-symbolic"
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    text: i18n("Previous")
                    enabled: card.player?.canGoPrevious ?? false
                    onClicked: card.player.Previous()
                }
                PlasmaComponents3.ToolButton {
                    icon.name: card.playing ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    text: card.playing ? i18n("Pause") : i18n("Play")
                    enabled: card.playing ? (card.player?.canPause ?? false) : (card.player?.canPlay ?? false)
                    onClicked: card.player.PlayPause()
                }
                PlasmaComponents3.ToolButton {
                    icon.name: "media-skip-forward-symbolic"
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    text: i18n("Next")
                    enabled: card.player?.canGoNext ?? false
                    onClicked: card.player.Next()
                }
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: if (card.player?.canRaise) card.player.Raise()
    }
}
