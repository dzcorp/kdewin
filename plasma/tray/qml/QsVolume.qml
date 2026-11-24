import QtQuick
import org.kde.plasma.private.volume

QuickSlider {
    id: volume

    GlobalConfig { id: config }

    readonly property var sink: PreferredDevice.sink
    readonly property real normal: PulseAudio.NormalVolume

    available: sink !== null && sink.name !== "auto_null"
    to: (config.raiseMaximumVolume ? PulseAudio.MaximalVolume : normal) / normal * 100
    stepSize: config.volumeStep > 0 ? config.volumeStep : 1
    value: sink ? sink.volume / normal * 100 : 0
    iconName: sink ? AudioIcon.forVolume(Math.round(value), sink.muted, "") : "audio-volume-muted"
    iconToolTip: sink?.muted ? i18n("Unmute") : i18n("Mute")
    hasDetails: true
    onMoved: v => {
        if (!sink) {
            return;
        }
        sink.volume = Math.round(v / 100 * normal);
        if (sink.muted && v > 0) {
            sink.muted = false;
        }
    }
    onIconClicked: if (sink) sink.muted = !sink.muted
    onDetailsRequested: qs.openPage("org.kde.plasma.volume")
}
