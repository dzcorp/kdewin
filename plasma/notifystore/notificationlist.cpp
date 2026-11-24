#include "notificationlist.h"

#include <QDateTime>

#include <algorithm>
#include <array>

namespace
{
// Roles passed through from the sources, by name; both models share the names they have in common.
constexpr std::array<const char *, 31> ForwardedRoles = {
    "notificationId", "summary", "body", "image", "type", "created", "updated", "iconName", "desktopEntry", "applicationName", "applicationIconName",
    "originName", "jobState", "percentage", "jobError", "suspendable", "killable", "jobDetails", "actionNames", "actionLabels", "hasDefaultAction",
    "configurable", "configureActionLabel", "closable", "hasReplyAction", "replyActionLabel", "replyPlaceholderText", "resident", "urgency", "urls", "key",
};

enum ExtraRole {
    KindRole = Qt::UserRole + 100,
    GroupKeyRole,
    GroupCountRole,
    GroupHiddenRole,
    GroupExpandedRole,
    FirstGroupRole,
    LeavingRole,
    UnreadRole,
};

constexpr int forwardedRole(int i)
{
    return Qt::UserRole + 1 + i;
}

int indexOfForwarded(const char *name)
{
    for (size_t i = 0; i < ForwardedRoles.size(); ++i) {
        if (qstrcmp(ForwardedRoles[i], name) == 0) {
            return int(i);
        }
    }
    return -1;
}

QVariant forwarded(const QList<QVariant> &values, int role)
{
    const int i = role - forwardedRole(0);
    return i >= 0 && i < values.size() ? values.at(i) : QVariant();
}

bool isHeader(const QString &id)
{
    return id.startsWith(QLatin1Char('h'));
}

const int IdIdx = indexOfForwarded("notificationId");
const int TypeIdx = indexOfForwarded("type");
const int CreatedIdx = indexOfForwarded("created");
const int DesktopEntryIdx = indexOfForwarded("desktopEntry");
const int AppNameIdx = indexOfForwarded("applicationName");
const int KeyIdx = indexOfForwarded("key");
const int UpdatedIdx = indexOfForwarded("updated");

qint64 stamp(const QList<QVariant> &values)
{
    const QDateTime updated = values.value(UpdatedIdx).toDateTime();
    return updated.isValid() ? updated.toMSecsSinceEpoch() : values.value(CreatedIdx).toDateTime().toMSecsSinceEpoch();
}

constexpr int JobType = 2; // NotificationManager::Notifications::JobType
}

NotificationList::NotificationList(QObject *parent)
    : QAbstractListModel(parent)
{
    m_expiry.setSingleShot(true);
    connect(&m_expiry, &QTimer::timeout, this, &NotificationList::sync);
    m_closeTimeout.setSingleShot(true);
    m_closeTimeout.setInterval(2000);
    connect(&m_closeTimeout, &QTimer::timeout, this, [this] {
        if (!m_closed.isEmpty()) {
            m_closed.clear();
            sync();
        }
    });
}

int NotificationList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int NotificationList::count() const
{
    return m_count;
}

int NotificationList::unread() const
{
    return m_unread;
}

bool NotificationList::isUnread(const QString &id, const Entry &e) const
{
    return e.source == Live && !id.startsWith(QLatin1Char('j')) && !m_closed.contains(id) && m_seen.value(id, -1) < stamp(e.values);
}

void NotificationList::countUnread()
{
    int n = 0;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        n += isUnread(it.key(), *it);
    }
    if (n != m_unread) {
        m_unread = n;
        Q_EMIT unreadChanged();
    }
}

void NotificationList::markSeen(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const QString &id = m_rows.at(row);
    const auto it = m_entries.constFind(id);
    if (it == m_entries.cend() || m_leaving.contains(id) || !isUnread(id, *it)) {
        return;
    }
    m_seen.insert(id, stamp(it->values));
    Q_EMIT dataChanged(index(row), index(row), {UnreadRole});
    countUnread();
}

QHash<int, QByteArray> NotificationList::roleNames() const
{
    QHash<int, QByteArray> names{
        {KindRole, "kind"},
        {GroupKeyRole, "groupKey"},
        {GroupCountRole, "groupCount"},
        {GroupHiddenRole, "groupHidden"},
        {GroupExpandedRole, "groupExpanded"},
        {FirstGroupRole, "firstGroup"},
        {LeavingRole, "leaving"},
        {UnreadRole, "unread"},
    };
    for (size_t i = 0; i < ForwardedRoles.size(); ++i) {
        names.insert(forwardedRole(int(i)), ForwardedRoles[i]);
    }
    return names;
}

QAbstractItemModel *NotificationList::model(int source) const
{
    return source == Live ? m_live.data() : m_earlier.data();
}

QModelIndex NotificationList::sourceIndexOf(const QString &id) const
{
    const auto it = m_entries.constFind(id);
    if (it == m_entries.cend()) {
        return {};
    }
    const QAbstractItemModel *m = model(it->source);
    return m ? m->index(it->row, 0) : QModelIndex();
}

QList<QVariant> NotificationList::read(int source, const QModelIndex &idx) const
{
    const QList<int> &map = m_roleMap[source == Live ? 0 : 1];
    QList<QVariant> values(map.size());
    for (int i = 0; i < map.size(); ++i) {
        if (map.at(i) >= 0) {
            values[i] = idx.data(map.at(i));
        }
    }
    return values;
}

QVariant NotificationList::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const QString &id = m_rows.at(index.row());

    if (const auto lit = m_leaving.constFind(id); lit != m_leaving.cend()) {
        switch (role) {
        case KindRole:
            return lit->kind;
        case GroupKeyRole:
            return lit->group;
        case GroupCountRole:
            return lit->count;
        case GroupHiddenRole:
            return lit->hidden;
        case GroupExpandedRole:
            return lit->expanded;
        case FirstGroupRole:
            return false;
        case LeavingRole:
            return true;
        case UnreadRole:
            return false;
        default:
            return forwarded(lit->values, role);
        }
    }
    if (role == LeavingRole) {
        return false;
    }

    if (!isHeader(id)) {
        const auto it = m_entries.constFind(id);
        if (it == m_entries.cend()) {
            return {};
        }
        switch (role) {
        case KindRole:
            return it->source;
        case GroupKeyRole:
            return it->group;
        case UnreadRole:
            return isUnread(id, *it);
        default:
            return forwarded(it->values, role);
        }
    }

    const QString key = id.mid(1);
    const auto git = m_groups.constFind(key);
    if (git == m_groups.cend()) {
        return {};
    }
    const Group &g = *git;
    switch (role) {
    case KindRole:
        return Header;
    case GroupKeyRole:
        return key;
    case GroupCountRole:
        return int(g.entries.size());
    case GroupHiddenRole:
        return g.hidden;
    case GroupExpandedRole:
        return g.expanded;
    case FirstGroupRole:
        return id == m_firstHeader;
    case UnreadRole:
        return false;
    default: {
        // App name and icon come from the group's newest notification.
        const auto it = g.entries.isEmpty() ? m_entries.cend() : m_entries.constFind(g.entries.first());
        return it == m_entries.cend() ? QVariant() : forwarded(it->values, role);
    }
    }
}

QAbstractItemModel *NotificationList::live() const
{
    return m_live;
}

void NotificationList::setLive(QAbstractItemModel *model)
{
    if (m_live != model) {
        attach(m_live, model);
        Q_EMIT liveChanged();
    }
}

QAbstractItemModel *NotificationList::earlier() const
{
    return m_earlier;
}

void NotificationList::setEarlier(QAbstractItemModel *model)
{
    if (m_earlier != model) {
        attach(m_earlier, model);
        Q_EMIT earlierChanged();
    }
}

void NotificationList::attach(QPointer<QAbstractItemModel> &slot, QAbstractItemModel *model)
{
    if (slot) {
        slot->disconnect(this);
    }
    slot = model;
    QList<int> &map = m_roleMap[&slot == &m_live ? 0 : 1];
    map.fill(-1, int(ForwardedRoles.size()));
    if (model) {
        const auto names = model->roleNames();
        for (auto it = names.cbegin(); it != names.cend(); ++it) {
            const int i = indexOfForwarded(it.value().constData());
            if (i >= 0) {
                map[i] = it.key();
            }
        }
        // Rows are refreshed synchronously: between a source change and our sync, cached source rows would be stale.
        connect(model, &QAbstractItemModel::rowsInserted, this, &NotificationList::sync);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &NotificationList::sync);
        connect(model, &QAbstractItemModel::rowsMoved, this, &NotificationList::sync);
        connect(model, &QAbstractItemModel::modelReset, this, &NotificationList::sync);
        connect(model, &QAbstractItemModel::layoutChanged, this, &NotificationList::sync);
        connect(model, &QAbstractItemModel::destroyed, this, &NotificationList::sync, Qt::QueuedConnection);
        connect(model, &QAbstractItemModel::dataChanged, this, [this, model](const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles) {
            onSourceDataChanged(model, tl, br, roles);
        });
    }
    sync();
}

void NotificationList::onSourceDataChanged(QAbstractItemModel *model, const QModelIndex &tl, const QModelIndex &br, const QList<int> &roles)
{
    const int source = model == m_live ? Live : Earlier;
    const QList<int> &map = m_roleMap[source == Live ? 0 : 1];
    QList<int> ours;
    for (const int r : roles) {
        const int i = int(map.indexOf(r));
        if (i >= 0) {
            ours.append(forwardedRole(i));
        }
    }
    if (!roles.isEmpty() && ours.isEmpty()) {
        return;
    }

    QSet<QString> changed;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->source != source || it->row < tl.row() || it->row > br.row()) {
            continue;
        }
        const QModelIndex idx = model->index(it->row, 0);
        if (ours.isEmpty()) {
            it->values = read(source, idx);
        } else {
            for (const int r : std::as_const(ours)) {
                const int i = r - forwardedRole(0);
                it->values[i] = idx.data(map.at(i));
            }
        }
        changed.insert(it.key());
    }
    if (changed.isEmpty()) {
        return;
    }
    if (!ours.isEmpty() && ours.contains(forwardedRole(UpdatedIdx))) {
        ours.append(UnreadRole);
    }
    countUnread();
    for (int row = 0; row < m_rows.size(); ++row) {
        const QString &id = m_rows.at(row);
        if (m_leaving.contains(id)) {
            continue;
        }
        const QString entry = isHeader(id) ? m_groups.value(id.mid(1)).entries.value(0) : id;
        if (changed.contains(entry)) {
            Q_EMIT dataChanged(index(row), index(row), ours);
        }
    }
}

bool NotificationList::frozen() const
{
    return m_frozen;
}

void NotificationList::setFrozen(bool frozen)
{
    if (m_frozen == frozen) {
        return;
    }
    m_frozen = frozen;
    if (!frozen) {
        for (Entry &e : m_entries) {
            e.pending = false;
        }
        sync();
    }
    Q_EMIT frozenChanged();
}

int NotificationList::groupLimit() const
{
    return m_groupLimit;
}

void NotificationList::setGroupLimit(int limit)
{
    if (m_groupLimit != limit) {
        m_groupLimit = std::max(1, limit);
        sync();
        Q_EMIT groupLimitChanged();
    }
}

int NotificationList::linger() const
{
    return m_linger;
}

void NotificationList::setLinger(int ms)
{
    ms = std::max(0, ms);
    if (m_linger == ms) {
        return;
    }
    m_linger = ms;
    if (ms == 0 && !m_leaving.isEmpty()) {
        sync();
    }
    Q_EMIT lingerChanged();
}

QModelIndex NotificationList::sourceIndex(int row) const
{
    return row >= 0 && row < m_rows.size() ? sourceIndexOf(m_rows.at(row)) : QModelIndex();
}

void NotificationList::setExpanded(const QString &group, bool expanded)
{
    m_expanded.insert(group, expanded);
    sync();
}

void NotificationList::collapseAll()
{
    if (!m_expanded.isEmpty()) {
        m_expanded.clear();
        sync();
    }
}

void NotificationList::closeAll()
{
    close([](const Entry &) {
        return true;
    });
}

void NotificationList::closeGroup(const QString &group)
{
    close([&group](const Entry &e) {
        return e.group == group;
    });
}

void NotificationList::close(const std::function<bool(const Entry &)> &match)
{
    // The sources drop closed rows in several steps; hiding them first keeps rows past the group limit from showing.
    QList<QPersistentModelIndex> live;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (match(*it)) {
            m_closed.insert(it.key());
            if (it->source == Live && m_live) {
                live.append(m_live->index(it->row, 0));
            }
        }
    }
    if (m_closed.isEmpty()) {
        return;
    }
    sync();
    for (const QPersistentModelIndex &idx : std::as_const(live)) {
        if (idx.isValid()) {
            QMetaObject::invokeMethod(m_live, "close", Q_ARG(QModelIndex, idx));
        }
    }
    m_closeTimeout.start();
}

void NotificationList::sync()
{
    QSet<QString> seen;
    QList<QString> restamped; // changed without a dataChanged from the source
    for (const int source : {Live, Earlier}) {
        const QAbstractItemModel *m = model(source);
        if (!m) {
            continue;
        }
        for (int row = 0, n = m->rowCount(); row < n; ++row) {
            const QModelIndex idx = m->index(row, 0);
            QList<QVariant> values = read(source, idx);
            const QString id = source == Earlier ? QLatin1Char('e') + values.at(KeyIdx).toString()
                                                 : (values.at(TypeIdx).toInt() == JobType ? QLatin1Char('j') : QLatin1Char('n')) + values.at(IdIdx).toString();
            seen.insert(id);
            auto it = m_entries.find(id);
            if (it != m_entries.end()) {
                if (stamp(values) != stamp(it->values)) {
                    restamped.append(id);
                }
                it->row = row;
                it->values = std::move(values);
                continue;
            }
            QString group = values.at(DesktopEntryIdx).toString();
            if (group.isEmpty()) {
                group = values.at(AppNameIdx).toString();
            }
            const qint64 created = values.at(CreatedIdx).toDateTime().toMSecsSinceEpoch();
            m_entries.insert(id, Entry{source, row, created, group, m_frozen && !m_rows.isEmpty(), false, std::move(values)});
        }
    }
    // Gone entries are kept for this pass: their rows may linger with what they last showed.
    QHash<QString, Entry> gone;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (seen.contains(it.key())) {
            ++it;
        } else {
            gone.insert(it.key(), std::move(*it));
            it = m_entries.erase(it);
        }
    }

    m_seen.removeIf([this](const auto &it) {
        return !m_entries.contains(it.key());
    });
    m_closed.removeIf([this](const QString &id) {
        return !m_entries.contains(id);
    });
    countUnread();

    const QHash<QString, Group> oldGroups = m_groups;
    for (Group &g : m_groups) {
        g.entries.clear();
    }
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        Group &g = m_groups[it->group];
        if (it->pending || m_closed.contains(it.key())) {
            continue;
        }
        if (!it->placed) {
            g.rank = std::max(g.rank, it->created);
            it->placed = true;
        }
        g.entries.append(it.key());
    }
    m_groups.removeIf([](const auto &it) {
        return it->entries.isEmpty();
    });

    QList<std::pair<qint64, QString>> order;
    order.reserve(m_groups.size());
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        order.append({-it->rank, it.key()});
    }
    std::sort(order.begin(), order.end());

    QList<QString> rows;
    rows.reserve(m_entries.size() + order.size());
    for (const auto &[negRank, key] : std::as_const(order)) {
        Group &g = m_groups[key];
        std::sort(g.entries.begin(), g.entries.end(), [this](const QString &a, const QString &b) {
            const qint64 ca = m_entries.constFind(a)->created;
            const qint64 cb = m_entries.constFind(b)->created;
            return ca != cb ? ca > cb : a > b;
        });
        g.expanded = m_expanded.value(key);
        const int shown = g.expanded ? int(g.entries.size()) : std::min(m_groupLimit, int(g.entries.size()));
        g.hidden = int(g.entries.size()) - shown;
        rows.append(QLatin1Char('h') + key);
        rows.append(g.entries.mid(0, shown));
    }
    apply(rows, gone, oldGroups);
    for (const QString &id : std::as_const(restamped)) {
        if (const int row = int(m_rows.indexOf(id)); row >= 0 && !m_leaving.contains(id)) {
            Q_EMIT dataChanged(index(row), index(row));
        }
    }
}

NotificationList::Leaving NotificationList::snapshot(const QString &id, const QHash<QString, Entry> &gone, const QHash<QString, Group> &oldGroups) const
{
    const auto entry = [&](const QString &key) {
        const auto it = m_entries.constFind(key);
        return it != m_entries.cend() ? *it : gone.value(key);
    };
    Leaving l{QDeadlineTimer(m_linger), Header, {}, {}};
    if (isHeader(id)) {
        const Group g = oldGroups.value(id.mid(1));
        l.group = id.mid(1);
        l.count = int(g.entries.size());
        l.hidden = g.hidden;
        l.expanded = g.expanded;
        l.values = entry(g.entries.value(0)).values;
    } else {
        const Entry e = entry(id);
        l.kind = e.source;
        l.group = e.group;
        l.values = e.values;
    }
    return l;
}

void NotificationList::apply(const QList<QString> &wantedRows, const QHash<QString, Entry> &gone, const QHash<QString, Group> &oldGroups)
{
    const QSet<QString> wanted(wantedRows.cbegin(), wantedRows.cend());
    QList<QString> started;
    QList<QString> revived;
    for (const QString &id : std::as_const(m_rows)) {
        if (wanted.contains(id)) {
            if (m_leaving.remove(id)) {
                revived.append(id);
            }
        } else if (m_linger > 0 && !m_leaving.contains(id)) {
            m_leaving.insert(id, snapshot(id, gone, oldGroups));
            started.append(id);
        }
    }
    m_leaving.removeIf([this](const auto &it) {
        return m_linger == 0 || it->until.hasExpired();
    });

    // A leaving row stays right after the row it followed, wherever that row went.
    QList<QString> target = wantedRows;
    if (!m_leaving.isEmpty()) {
        int last = -1;
        for (const QString &id : std::as_const(m_rows)) {
            if (m_leaving.contains(id)) {
                target.insert(++last, id);
            } else if (const int at = int(target.indexOf(id)); at >= 0) {
                last = at;
            }
        }
    }

    const QSet<QString> keep(target.cbegin(), target.cend());
    for (int i = int(m_rows.size()) - 1; i >= 0; --i) {
        if (keep.contains(m_rows.at(i))) {
            continue;
        }
        const int last = i;
        while (i > 0 && !keep.contains(m_rows.at(i - 1))) {
            --i;
        }
        beginRemoveRows({}, i, last);
        m_rows.remove(i, last - i + 1);
        endRemoveRows();
    }

    QSet<QString> present(m_rows.cbegin(), m_rows.cend());
    for (int i = 0; i < target.size(); ++i) {
        if (i < m_rows.size() && m_rows.at(i) == target.at(i)) {
            continue;
        }
        if (present.contains(target.at(i))) {
            // Everything before i already matches, so the row sits further down: move it up with its run.
            const int from = int(m_rows.indexOf(target.at(i), i));
            int len = 1;
            while (from + len < m_rows.size() && i + len < target.size() && m_rows.at(from + len) == target.at(i + len)) {
                ++len;
            }
            beginMoveRows({}, from, from + len - 1, {}, i);
            std::rotate(m_rows.begin() + i, m_rows.begin() + from, m_rows.begin() + from + len);
            endMoveRows();
            i += len - 1;
        } else {
            int len = 1;
            while (i + len < target.size() && !present.contains(target.at(i + len))) {
                ++len;
            }
            beginInsertRows({}, i, i + len - 1);
            for (int k = 0; k < len; ++k) {
                m_rows.insert(i + k, target.at(i + k));
                present.insert(target.at(i + k));
            }
            endInsertRows();
            i += len - 1;
        }
    }

    m_firstHeader.clear();
    int count = 0;
    for (const QString &id : std::as_const(m_rows)) {
        if (!m_leaving.contains(id)) {
            ++count;
            if (m_firstHeader.isEmpty() && isHeader(id)) {
                m_firstHeader = id;
            }
        }
    }

    // Header roles depend on the whole group and on position.
    for (int row = 0; row < m_rows.size(); ++row) {
        const QString &id = m_rows.at(row);
        if (isHeader(id) && !m_leaving.contains(id)) {
            Q_EMIT dataChanged(index(row), index(row), {GroupCountRole, GroupHiddenRole, GroupExpandedRole, FirstGroupRole});
        }
    }
    for (const QString &id : std::as_const(started)) {
        if (const int row = int(m_rows.indexOf(id)); row >= 0) {
            Q_EMIT dataChanged(index(row), index(row), {LeavingRole, FirstGroupRole});
        }
    }
    for (const QString &id : std::as_const(revived)) {
        if (const int row = int(m_rows.indexOf(id)); row >= 0) {
            Q_EMIT dataChanged(index(row), index(row));
        }
    }

    qint64 next = -1;
    for (const Leaving &l : std::as_const(m_leaving)) {
        const qint64 left = l.until.remainingTime();
        next = next < 0 ? left : std::min(next, left);
    }
    if (next >= 0) {
        m_expiry.start(int(next) + 1);
    } else {
        m_expiry.stop();
    }

    if (count != m_count) {
        m_count = count;
        Q_EMIT countChanged();
    }
}
