#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QTimer>
#include <QtQmlIntegration>

/// Notification Center history that survives plasmashell restarts and re-login.
/// libnotificationmanager keeps history in memory only and cannot re-add past notifications,
/// so earlier sessions are listed on their own (without actions: their senders are gone).
class NotificationStore : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        SummaryRole,
        BodyRole,
        ApplicationNameRole,
        ApplicationIconNameRole,
        IconNameRole,
        DesktopEntryRole,
        CreatedRole,
    };
    Q_ENUM(Role)

    explicit NotificationStore(QObject *parent = nullptr);
    ~NotificationStore() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Keeps (or updates) a notification of this session; it is listed after the next restart.
    Q_INVOKABLE void record(const QVariantMap &notification);
    Q_INVOKABLE void remove(const QString &key);
    /// Forgets an app's notifications; the group is its desktop entry, or its name when it has none.
    Q_INVOKABLE void removeGroup(const QString &group);
    Q_INVOKABLE void clear();
    /// Opens the sending application, then forgets the notification.
    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE static QString keyOf(uint notificationId, const QDateTime &created);

Q_SIGNALS:
    void countChanged();

private:
    struct Entry {
        QString key;
        QString summary;
        QString body;
        QString app;
        QString appIcon;
        QString icon;
        QString desktopEntry;
        QDateTime created;
    };

    void load();
    void scheduleSave();
    void save() const;
    void removeIf(const std::function<bool(const Entry &)> &match);

    QList<Entry> m_session; // recorded in this session, kept for the next one
    QList<Entry> m_earlier; // previous sessions, newest first: the model rows
    QTimer m_saveTimer;
};
