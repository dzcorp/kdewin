import QtQuick
import org.kde.notificationmanager as NotificationManager

QuickTile {
    NotificationManager.Settings { id: settings }

    readonly property bool dnd: !isNaN(settings.notificationsInhibitedUntil.getTime()) && Date.now() < settings.notificationsInhibitedUntil.getTime()

    text: i18n("Do not disturb")
    iconName: checked ? "notifications-disabled-symbolic" : "notifications-symbolic"
    available: true
    checked: dnd || settings.notificationsInhibitedByApplication
    onToggled: {
        if (dnd || settings.notificationsInhibitedByApplication) {
            settings.notificationsInhibitedUntil = undefined;
            settings.revokeApplicationInhibitions();
        } else {
            const until = new Date();
            until.setFullYear(until.getFullYear() + 1);
            settings.notificationsInhibitedUntil = until;
        }
        settings.save();
    }
}
