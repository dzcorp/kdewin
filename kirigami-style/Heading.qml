import QtQuick
import org.kde.kirigami.platform as Platform
import org.kde.kirigami.templates as KT

// Kirigami's heading scale, except that settings apps title their pages (level 1) like Windows Settings.
KT.Heading {
    id: heading

    readonly property bool __pageTitle: level === 1 && ["systemsettings", "kcmshell6", "kinfocenter"].includes(Qt.application.name)
    readonly property var __title: [@@type.title@@]

    font.family: __pageTitle ? "@@font.ui_display@@" : Platform.Theme.defaultFont.family
    font.pointSize: Platform.Theme.defaultFont.pointSize * (__pageTitle ? __title[0] / [@@type.body@@][0] : [1.35, 1.20, 1.15, 1.10][level - 1] ?? 1)
    font.weight: __pageTitle ? __title[2] : type === KT.Heading.Type.Primary ? Font.DemiBold : Font.Normal

    opacity: type === KT.Heading.Type.Secondary ? 0.75 : 1
}
