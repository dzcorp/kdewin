import QtQuick
import QtQuick.Shapes

// Fluent-style network, volume and battery glyphs on a 16 px grid, picked from an applet's icon name.
// `spec` is null for names without a glyph here.
Item {
    id: glyph

    property string iconName
    readonly property var spec: parse(iconName)

    readonly property string speaker: "M0.5 5.5H2.5L5.5 2.5V13.5L2.5 10.5H0.5Z"
    readonly property var waves: ["M7.43 5.7A3 3 0 0 1 7.43 10.3", "M9.36 3.4A6 6 0 0 1 9.36 12.6", "M11.29 1.11A9 9 0 0 1 11.29 14.89"]
    readonly property var arcs: ["M5.53 10.53A3.5 3.5 0 0 1 10.47 10.53", "M3.05 8.05A7 7 0 0 1 12.95 8.05", "M0.58 5.58A10.5 10.5 0 0 1 15.42 5.58"]
    readonly property var bars: ["M1 11H3V14H1Z", "M5 8H7V14H5Z", "M9 5H11V14H9Z", "M13 2H15V14H13Z"]

    function parse(name: string): var {
        let m = /^audio-volume-(muted|low|medium|high)/.exec(name);
        if (m) {
            const n = ["muted", "low", "medium", "high"].indexOf(m[1]);
            return { round: true, stroke: speaker + (n === 0 ? "M9.5 5.5L14.5 10.5M14.5 5.5L9.5 10.5" : waves.slice(0, n).join("")) };
        }
        if (/^(network-)?bluetooth|^preferences-system-bluetooth/.test(name)) {
            return { round: true, stroke: "M3.5 4.5L11 11L7.5 14.5V1.5L11 5L3.5 11.5" };
        }
        if (/^audio-input-microphone/.test(name)) {
            return { round: true, stroke: "M7.5 0.5A2 2 0 0 1 9.5 2.5V7.5A2 2 0 0 1 5.5 7.5V2.5A2 2 0 0 1 7.5 0.5ZM3.5 7A4 4 0 0 0 11.5 7M7.5 11V15M4.5 15.5H10.5"
                + (/-muted/.test(name) ? "M1.5 1.5L14.5 14.5" : "") };
        }
        if (/^(device-notifier|media-removable|drive-removable)/.test(name)) {
            return { stroke: "M5.5 4.5V0.5H10.5V4.5M4.5 4.5H11.5A1 1 0 0 1 12.5 5.5V14.5A1 1 0 0 1 11.5 15.5H4.5A1 1 0 0 1 3.5 14.5V5.5A1 1 0 0 1 4.5 4.5Z",
                fill: "M6 2H7V3H6ZM9 2H10V3H9Z" };
        }
        if (/^printer/.test(name)) {
            return { stroke: "M4.5 5.5V1.5H11.5V5.5M4.5 11.5H2.5A1 1 0 0 1 1.5 10.5V6.5A1 1 0 0 1 2.5 5.5H13.5A1 1 0 0 1 14.5 6.5V10.5"
                + "A1 1 0 0 1 13.5 11.5H11.5M4.5 9.5H11.5V14.5H4.5Z" };
        }
        if (/^(klipper|edit-paste|clipboard)/.test(name)) {
            return { stroke: "M5 2.5H3.5A1 1 0 0 0 2.5 3.5V14.5A1 1 0 0 0 3.5 15.5H12.5A1 1 0 0 0 13.5 14.5V3.5A1 1 0 0 0 12.5 2.5H11M5.5 1.5H10.5V4.5H5.5Z" };
        }
        if (/^(kdeconnect|smartphone|phone)/.test(name)) {
            return { round: true, stroke: "M6 0.5H10A1.5 1.5 0 0 1 11.5 2V14A1.5 1.5 0 0 1 10 15.5H6A1.5 1.5 0 0 1 4.5 14V2A1.5 1.5 0 0 1 6 0.5ZM7 13.5H9" };
        }
        if (/^network-(wired|wireless)-disconnected|^network-(unavailable|disconnect)|-limited/.test(name)) {
            return { round: true, stroke: "M7.5 13.5A6 6 0 1 1 13.5 7.5M7.5 13.5A2.5 6 0 1 1 9.98 8.18M1.5 7.5H13.5"
                + "M15.5 12.5A3 3 0 1 1 9.5 12.5A3 3 0 1 1 15.5 12.5ZM10.38 10.38L14.62 14.62" };
        }
        if (/^network-wired/.test(name)) {
            return { stroke: "M10 0.5H1.5A1 1 0 0 0 0.5 1.5V11.5A1 1 0 0 0 1.5 12.5H12M12.5 0.5H14.5A1 1 0 0 1 15.5 1.5V5.5"
                + "A1 1 0 0 1 14.5 6.5H12.5A1 1 0 0 1 11.5 5.5V1.5A1 1 0 0 1 12.5 0.5ZM11.5 2.5H15.5M13.5 7V16M5.5 13V15M10.5 13V15M3 15.5H12" };
        }
        m = /^network-wireless-(?:connected-)?(\d+)/.exec(name);
        if (m) {
            const n = m[1] >= 70 ? 3 : m[1] >= 50 ? 2 : m[1] >= 20 ? 1 : 0;
            return { round: true, stroke: arcs.slice(0, n).join(""), dim: arcs.slice(n).join(""),
                fill: "M9.25 13A1.25 1.25 0 1 1 6.75 13A1.25 1.25 0 1 1 9.25 13Z" };
        }
        m = /^network-mobile-(\d+)/.exec(name);
        if (m) {
            const n = Math.round(m[1] / 25);
            return { fill: bars.slice(0, n).join(""), dimFill: bars.slice(n).join("") };
        }
        if (/^network-flightmode-on/.test(name)) {
            return { fill: "M8 0.5C8.7 0.5 9 1.3 9 2V6L15.5 9.75V11.25L9 9.5V12.75L10.75 14.25V15.25L8 14.5L5.25 15.25V14.25L7 12.75"
                + "V9.5L0.5 11.25V9.75L7 6V2C7 1.3 7.3 0.5 8 0.5Z" };
        }
        m = /^battery-(?:level-)?(\d+|full)(-charg)?/.exec(name);
        if (m) {
            const w = m[1] === "full" ? 10 : Math.round(m[1] / 10);
            return { stroke: "M2 4.5H12A1.5 1.5 0 0 1 13.5 6V10A1.5 1.5 0 0 1 12 11.5H2A1.5 1.5 0 0 1 0.5 10V6A1.5 1.5 0 0 1 2 4.5ZM14.5 6.5V9.5",
                fill: m[2] ? "M8.5 5.5L4.5 8.75H7L5.5 10.5L9.5 7.25H7Z" : w > 0 ? `M2 6H${2 + w}V10H2Z` : "" };
        }
        return null;
    }

    implicitWidth: 16
    implicitHeight: 16

    Shape {
        anchors.centerIn: parent
        width: 16
        height: 16
        scale: Math.min(glyph.width, glyph.height) / 16
        visible: glyph.spec !== null
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: lt.textDisabled
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: glyph.spec?.dim ?? "" }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: lt.textDisabled
            PathSvg { path: glyph.spec?.dimFill ?? "" }
        }
        ShapePath {
            strokeColor: lt.textPrimary
            fillColor: "transparent"
            capStyle: glyph.spec?.round ? ShapePath.RoundCap : ShapePath.FlatCap
            joinStyle: ShapePath.MiterJoin
            PathSvg { path: glyph.spec?.stroke ?? "" }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: lt.textPrimary
            PathSvg { path: glyph.spec?.fill ?? "" }
        }
    }
}
