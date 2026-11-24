#pragma once

#include <QAbstractListModel>
#include <QDeadlineTimer>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QtQmlIntegration>

#include <functional>

/// Notification Center rows: an app header followed by that app's notifications, live and from earlier sessions.
/// The order is stable: closing never reorders anything, and while frozen (pointer over the list) new arrivals
/// wait, so the row under the pointer only changes when the user closes it and the next one slides in.
/// Rows that go away stay listed for `linger` ms, flagged `leaving` and showing their last data, so the view can
/// animate them out while it keeps laying out every row.
class NotificationList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QAbstractItemModel *live READ live WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(QAbstractItemModel *earlier READ earlier WRITE setEarlier NOTIFY earlierChanged)
    Q_PROPERTY(bool frozen READ frozen WRITE setFrozen NOTIFY frozenChanged)
    Q_PROPERTY(int groupLimit READ groupLimit WRITE setGroupLimit NOTIFY groupLimitChanged)
    Q_PROPERTY(int linger READ linger WRITE setLinger NOTIFY lingerChanged)
    /// Rows that are not leaving.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /// Live notifications the user has not seen in the list since they arrived or last changed.
    Q_PROPERTY(int unread READ unread NOTIFY unreadChanged)

public:
    enum Kind {
        Header,
        Live,
        Earlier,
    };
    Q_ENUM(Kind)

    explicit NotificationList(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QAbstractItemModel *live() const;
    void setLive(QAbstractItemModel *model);
    QAbstractItemModel *earlier() const;
    void setEarlier(QAbstractItemModel *model);
    bool frozen() const;
    void setFrozen(bool frozen);
    int groupLimit() const;
    void setGroupLimit(int limit);
    int linger() const;
    void setLinger(int ms);
    int count() const;
    int unread() const;

    /// The row's index in its source model, resolved now: source rows shift as notifications come and go.
    Q_INVOKABLE QModelIndex sourceIndex(int row) const;
    Q_INVOKABLE void setExpanded(const QString &group, bool expanded);
    Q_INVOKABLE void collapseAll();
    /// Hides the group at once and closes its live notifications; earlier ones belong to the store.
    Q_INVOKABLE void closeGroup(const QString &group);
    Q_INVOKABLE void closeAll();
    /// The row was on screen long enough to be read.
    Q_INVOKABLE void markSeen(int row);

Q_SIGNALS:
    void liveChanged();
    void earlierChanged();
    void frozenChanged();
    void groupLimitChanged();
    void lingerChanged();
    void countChanged();
    void unreadChanged();

private:
    struct Entry {
        int source; // Live or Earlier
        int row; // in its source, refreshed on every sync
        qint64 created;
        QString group;
        bool pending; // arrived while frozen
        bool placed; // already counted in its group's rank
        QList<QVariant> values; // forwarded roles
    };
    struct Group {
        qint64 rank = 0; // newest arrival; closing never lowers it, so groups never reorder on close
        bool expanded = false;
        QList<QString> entries; // newest first, excluding pending ones
        int hidden = 0;
    };
    /// What a leaving row last showed.
    struct Leaving {
        QDeadlineTimer until;
        int kind;
        QString group;
        QList<QVariant> values;
        int count = 0;
        int hidden = 0;
        bool expanded = false;
    };

    void attach(QPointer<QAbstractItemModel> &slot, QAbstractItemModel *model);
    void onSourceDataChanged(QAbstractItemModel *model, const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles);
    void close(const std::function<bool(const Entry &)> &match);
    void sync();
    void apply(const QList<QString> &rows, const QHash<QString, Entry> &gone, const QHash<QString, Group> &oldGroups);
    Leaving snapshot(const QString &id, const QHash<QString, Entry> &gone, const QHash<QString, Group> &oldGroups) const;
    QList<QVariant> read(int source, const QModelIndex &idx) const;
    QModelIndex sourceIndexOf(const QString &id) const;
    bool isUnread(const QString &id, const Entry &e) const;
    void countUnread();
    QAbstractItemModel *model(int source) const;

    QPointer<QAbstractItemModel> m_live;
    QPointer<QAbstractItemModel> m_earlier;
    QList<int> m_roleMap[2]; // our role index -> source role, -1 if the source has none
    QHash<QString, Entry> m_entries;
    QHash<QString, Group> m_groups;
    QHash<QString, bool> m_expanded;
    QHash<QString, Leaving> m_leaving;
    QSet<QString> m_closed; // closed by the user: hidden until their source drops them
    QHash<QString, qint64> m_seen; // entry id -> the stamp (updated, else created) it had when seen
    QList<QString> m_rows; // "h<group>" for headers, entry ids otherwise
    QString m_firstHeader; // first header that is not leaving
    QTimer m_expiry;
    QTimer m_closeTimeout; // a close that did nothing brings its rows back
    bool m_frozen = false;
    int m_groupLimit = 3;
    int m_linger = 0;
    int m_count = 0;
    int m_unread = 0;
};
