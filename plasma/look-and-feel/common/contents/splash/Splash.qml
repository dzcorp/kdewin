import QtCore
import QtQuick

// KSplash: plasmashell advances `stage` 1..6; fade out once the desktop is ready.
Rectangle {
    id: root
    color: "#000000"

    property int stage

    LumenTokens { id: lt; reference: root.color }

    onStageChanged: {
        if (stage === 2) {
            intro.start()
        } else if (stage === 6) {
            outro.start()
        }
    }

    Item {
        id: content
        anchors.fill: parent
        opacity: 0

        Image {
            id: wallpaper
            anchors.fill: parent
            source: StandardPaths.locate(StandardPaths.GenericDataLocation, "wallpapers/Lumen/contents/images_dark/3840x2160.png")
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: 0.35
        }

        Column {
            anchors.centerIn: parent
            spacing: 28

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Welcome")
                color: "#ffffff"
                font.family: lt.fontUiDisplay
                font.pixelSize: lt.typeTitle[0]
                font.weight: Font.DemiBold
            }

            // Boot dots: same motion as the Plymouth throbber (see boot.rs spin_path).
            Item {
                id: ring
                anchors.horizontalCenter: parent.horizontalCenter
                width: 40
                height: 40
                property real t: 0

                NumberAnimation on t {
                    from: 0
                    to: 1
                    duration: lt.bootSpinMs
                    loops: Animation.Infinite
                    running: content.opacity > 0
                }

                Repeater {
                    model: lt.bootSpinDots
                    Rectangle {
                        required property int index
                        readonly property real u: (ring.t - index * lt.bootSpinLag) / lt.bootSpinLife
                        readonly property real angle: {
                            const p = lt.bootSpinPath
                            const x = Math.max(0, Math.min(1, u)) * (p.length - 1)
                            const i = Math.min(Math.floor(x), p.length - 2)
                            return (180 - index * lt.bootSpinTrail + p[i] + (p[i + 1] - p[i]) * (x - i)) * Math.PI / 180
                        }
                        readonly property real orbit: ring.width * 0.42
                        visible: u >= 0 && u <= 1
                        width: orbit * 0.304
                        height: width
                        radius: width / 2
                        color: "#ffffff"
                        x: ring.width / 2 + orbit * Math.sin(angle) - width / 2
                        y: ring.height / 2 - orbit * Math.cos(angle) - height / 2
                    }
                }
            }
        }
    }

    OpacityAnimator {
        id: intro
        target: content
        from: 0
        to: 1
        duration: lt.motionNormalMs
        easing.type: lt.easingDecel
    }

    OpacityAnimator {
        id: outro
        target: root
        from: 1
        to: 0
        duration: lt.motionSlowMs
        easing.type: lt.easingAccel
    }
}
