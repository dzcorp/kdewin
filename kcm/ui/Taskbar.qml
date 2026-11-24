import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

LumenPage {
    id: taskbarPage
    title: i18n("Taskbar")

    readonly property var bar: kcm.taskbar
    function set(key, value) {
        kcm.setTaskbarValue(key, value);
    }

    LumenCard {
        visible: !kcm.taskbarFound
        FormCard.FormPlaceholderMessageDelegate {
            icon.name: "dialog-information"
            text: i18n("The Lumen taskbar is not on this desktop")
            explanation: i18n("Apply the Lumen layout to replace the current panel. Your widgets, tray items, pinned apps and favorites are kept.")
        }
        ToolAction { text: i18n("Apply Lumen layout"); args: ["apply", kcm.settings.appearanceMode, "--layout"] }
    }

    LumenHeader { visible: kcm.taskbarFound; title: i18n("Layout") }
    LumenCard {
        visible: kcm.taskbarFound
        FormCard.FormSwitchDelegate {
            text: i18n("Center taskbar icons")
            checked: taskbarPage.bar.centered
            onToggled: taskbarPage.set("centered", checked)
        }
        FormCard.FormSwitchDelegate {
            text: i18n("Floating")
            checked: taskbarPage.bar.floating
            onToggled: taskbarPage.set("floating", checked)
        }
        FormCard.FormSwitchDelegate {
            text: i18n("Automatically hide")
            checked: taskbarPage.bar.autoHide
            onToggled: taskbarPage.set("autoHide", checked)
        }
    }

    LumenHeader { visible: kcm.taskbarFound; title: i18n("Icons") }
    LumenCard {
        visible: kcm.taskbarFound
        FormCard.FormSliderDelegate {
            label: i18n("Taskbar height")
            from: 32
            to: 80
            stepSize: 2
            value: taskbarPage.bar.height
            valueText: Math.round(value) + " px"
            onMoved: taskbarPage.set("height", Math.round(value))
        }
        FormCard.FormSliderDelegate {
            label: i18n("Start icon size")
            from: 16
            to: Math.max(16, Math.min(64, taskbarPage.bar.height - 2))
            stepSize: 2
            value: taskbarPage.bar.startIcon
            valueText: Math.round(value) + " px"
            onMoved: taskbarPage.set("startIcon", Math.round(value))
        }
        FormCard.FormSliderDelegate {
            label: i18n("App icon size")
            description: i18n("Start and app icons keep this size at any taskbar height")
            from: 16
            to: Math.max(16, Math.min(64, taskbarPage.bar.height - 2))
            stepSize: 2
            value: taskbarPage.bar.appIcon
            valueText: Math.round(value) + " px"
            onMoved: taskbarPage.set("appIcon", Math.round(value))
        }
        FormCard.FormComboBoxDelegate {
            text: i18n("Space between apps")
            model: [i18n("None"), i18n("Normal"), i18n("Wide"), i18n("Extra wide")]
            currentIndex: taskbarPage.bar.tasksSpacing
            onActivated: index => taskbarPage.set("tasksSpacing", index)
        }
        FormCard.FormDelegateSeparator {}
        FormCard.FormSliderDelegate {
            label: i18n("Tray icon size")
            from: 16
            to: 32
            stepSize: 2
            value: taskbarPage.bar.trayIcon
            valueText: Math.round(value) + " px"
            onMoved: taskbarPage.set("trayIcon", Math.round(value))
        }
        FormCard.FormSliderDelegate {
            label: i18n("Quick Settings icon size")
            description: i18n("Network, volume and battery next to the clock")
            from: 16
            to: 32
            stepSize: 2
            value: taskbarPage.bar.quickIcon
            valueText: Math.round(value) + " px"
            onMoved: taskbarPage.set("quickIcon", Math.round(value))
        }
        FormCard.FormSliderDelegate {
            label: i18n("Space between tray icons")
            from: 0
            to: 6
            stepSize: 1
            value: taskbarPage.bar.traySpacing
            valueText: (16 + Math.round(value) * 4) + " px"
            onMoved: taskbarPage.set("traySpacing", Math.round(value))
        }
    }

    LumenHeader { visible: kcm.taskbarFound; title: i18n("Clock") }
    LumenCard {
        visible: kcm.taskbarFound
        FormCard.FormComboBoxDelegate {
            text: i18n("Show")
            model: [i18n("Time over date"), i18n("Time and date on one line"), i18n("Time only"), i18n("Date only")]
            currentIndex: taskbarPage.bar.clockMode
            onActivated: index => taskbarPage.set("clockMode", index)
        }
        FormCard.FormComboBoxDelegate {
            text: i18n("Time format")
            model: [i18n("Use region settings"), i18n("12-hour"), i18n("24-hour")]
            currentIndex: taskbarPage.bar.clockHour
            onActivated: index => taskbarPage.set("clockHour", index)
        }
        FormCard.FormComboBoxDelegate {
            readonly property date sample: new Date()
            text: i18n("Date format")
            model: [
                i18n("Region short (%1)", sample.toLocaleDateString(Qt.locale(), Locale.ShortFormat)),
                i18n("Weekday (%1)", Qt.locale().toString(sample, "ddd d MMM")),
                i18n("Region long (%1)", sample.toLocaleDateString(Qt.locale(), Locale.LongFormat)),
                i18n("ISO (%1)", Qt.locale().toString(sample, "yyyy-MM-dd")),
                i18n("Custom (set in the clock's settings)"),
            ]
            currentIndex: taskbarPage.bar.clockDateStyle
            onActivated: index => taskbarPage.set("clockDateStyle", index)
        }
        FormCard.FormSwitchDelegate {
            text: i18n("Show seconds")
            checked: taskbarPage.bar.clockSeconds
            onToggled: taskbarPage.set("clockSeconds", checked)
        }
        FormCard.FormSwitchDelegate {
            text: i18n("Bold time")
            checked: taskbarPage.bar.clockBold
            onToggled: taskbarPage.set("clockBold", checked)
        }
        FormCard.FormSliderDelegate {
            label: i18n("Time size")
            from: 70
            to: 200
            stepSize: 5
            value: taskbarPage.bar.clockTimeSize
            valueText: Math.round(value) + "%"
            onMoved: taskbarPage.set("clockTimeSize", Math.round(value))
        }
        FormCard.FormComboBoxDelegate {
            text: i18n("Time alignment")
            model: [i18n("Left"), i18n("Center"), i18n("Right")]
            currentIndex: taskbarPage.bar.clockTimeAlign
            onActivated: index => taskbarPage.set("clockTimeAlign", index)
        }
        FormCard.FormSliderDelegate {
            label: i18n("Date size")
            description: i18n("Large sizes shrink to fit; the taskbar itself never changes")
            from: 70
            to: 200
            stepSize: 5
            value: taskbarPage.bar.clockDateSize
            valueText: Math.round(value) + "%"
            onMoved: taskbarPage.set("clockDateSize", Math.round(value))
        }
        FormCard.FormComboBoxDelegate {
            text: i18n("Date alignment")
            model: [i18n("Left"), i18n("Center"), i18n("Right")]
            currentIndex: taskbarPage.bar.clockDateAlign
            onActivated: index => taskbarPage.set("clockDateAlign", index)
        }
    }

    LumenHeader { visible: kcm.taskbarFound; title: i18n("Parts") }
    LumenCard {
        visible: kcm.taskbarFound
        Repeater {
            model: [
                { id: "org.lumen.start", icon: "start-here-kde-symbolic", label: i18n("Start"), help: i18n("Pinned and recommended apps, power and account menus") },
                { id: "org.lumen.tray", icon: "preferences-desktop-notification", label: i18n("Tray and Quick Settings"), help: i18n("Icons, overflow, tiles and sliders") },
                { id: "org.lumen.clock", icon: "preferences-system-time", label: i18n("Clock and notifications"), help: i18n("Custom date format, calendar and Notification Center") },
                { id: "org.lumen.tasks", icon: "preferences-system-windows", label: i18n("App buttons"), help: i18n("Pinned apps and running windows") },
            ]
            delegate: FormCard.FormButtonDelegate {
                required property var modelData
                icon.name: modelData.icon
                text: modelData.label
                description: modelData.help
                onClicked: kcm.configureWidget(modelData.id)
            }
        }
    }

    LumenHeader { title: i18n("Notifications") }
    LumenCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Show over full-screen apps")
            description: i18n("Off: full-screen games and videos turn on Do Not Disturb (critical notifications still show)")
            checked: kcm.notifyOverFullscreen
            onToggled: kcm.notifyOverFullscreen = checked
        }
    }
}
