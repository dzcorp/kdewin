#include "notificationstore.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
constexpr int MaxEntries = 100;
constexpr int MaxAgeDays = 30;

QString storePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/lumen/notifications.json");
}
}

NotificationStore::NotificationStore(QObject *parent)
    : QAbstractListModel(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(1000);
    connect(&m_saveTimer, &QTimer::timeout, this, &NotificationStore::save);
    load();
}

NotificationStore::~NotificationStore()
{
    if (m_saveTimer.isActive()) {
        save();
    }
}

QString NotificationStore::keyOf(uint notificationId, const QDateTime &created)
{
    return QString::number(created.toMSecsSinceEpoch()) + QLatin1Char('/') + QString::number(notificationId);
}

int NotificationStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_earlier.size());
}

QVariant NotificationStore::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Entry &e = m_earlier.at(index.row());
    switch (role) {
    case KeyRole:
        return e.key;
    case SummaryRole:
        return e.summary;
    case BodyRole:
        return e.body;
    case ApplicationNameRole:
        return e.app;
    case ApplicationIconNameRole:
        return e.appIcon;
    case IconNameRole:
        return e.icon;
    case DesktopEntryRole:
        return e.desktopEntry;
    case CreatedRole:
        return e.created;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotificationStore::roleNames() const
{
    return {
        {KeyRole, "key"},
        {SummaryRole, "summary"},
        {BodyRole, "body"},
        {ApplicationNameRole, "applicationName"},
        {ApplicationIconNameRole, "applicationIconName"},
        {IconNameRole, "iconName"},
        {DesktopEntryRole, "desktopEntry"},
        {CreatedRole, "created"},
    };
}

void NotificationStore::record(const QVariantMap &n)
{
    const QDateTime created = n.value(QStringLiteral("created")).toDateTime();
    if (!created.isValid()) {
        return;
    }
    Entry e{
        keyOf(n.value(QStringLiteral("notificationId")).toUInt(), created),
        n.value(QStringLiteral("summary")).toString(),
        n.value(QStringLiteral("body")).toString(),
        n.value(QStringLiteral("applicationName")).toString(),
        n.value(QStringLiteral("applicationIconName")).toString(),
        n.value(QStringLiteral("iconName")).toString(),
        n.value(QStringLiteral("desktopEntry")).toString(),
        created,
    };
    const auto it = std::find_if(m_session.begin(), m_session.end(), [&](const Entry &s) {
        return s.key == e.key;
    });
    if (it != m_session.end()) {
        *it = std::move(e);
    } else {
        m_session.prepend(std::move(e));
    }
    scheduleSave();
}

void NotificationStore::removeIf(const std::function<bool(const Entry &)> &match)
{
    const qsizetype before = m_session.size() + m_earlier.size();
    m_session.removeIf(match);
    for (int row = int(m_earlier.size()) - 1; row >= 0; --row) {
        if (match(m_earlier.at(row))) {
            beginRemoveRows({}, row, row);
            m_earlier.removeAt(row);
            endRemoveRows();
        }
    }
    if (m_session.size() + m_earlier.size() != before) {
        Q_EMIT countChanged();
        scheduleSave();
    }
}

void NotificationStore::remove(const QString &key)
{
    removeIf([&](const Entry &e) {
        return e.key == key;
    });
}

void NotificationStore::removeGroup(const QString &group)
{
    removeIf([&](const Entry &e) {
        return (e.desktopEntry.isEmpty() ? e.app : e.desktopEntry) == group;
    });
}

void NotificationStore::clear()
{
    beginResetModel();
    m_session.clear();
    m_earlier.clear();
    endResetModel();
    Q_EMIT countChanged();
    scheduleSave();
}

void NotificationStore::launch(int row)
{
    if (row < 0 || row >= m_earlier.size()) {
        return;
    }
    const Entry e = m_earlier.at(row);
    if (const KService::Ptr service = KService::serviceByDesktopName(e.desktopEntry)) {
        (new KIO::ApplicationLauncherJob(service))->start();
    }
    remove(e.key);
}

void NotificationStore::load()
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QDateTime oldest = QDateTime::currentDateTime().addDays(-MaxAgeDays);
    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        Entry e{
            o.value(u"key").toString(),
            o.value(u"summary").toString(),
            o.value(u"body").toString(),
            o.value(u"app").toString(),
            o.value(u"appIcon").toString(),
            o.value(u"icon").toString(),
            o.value(u"desktopEntry").toString(),
            QDateTime::fromMSecsSinceEpoch(qint64(o.value(u"created").toDouble())),
        };
        if (e.created >= oldest && m_earlier.size() < MaxEntries) {
            m_earlier.append(std::move(e));
        }
    }
    std::stable_sort(m_earlier.begin(), m_earlier.end(), [](const Entry &a, const Entry &b) {
        return a.created > b.created;
    });
}

void NotificationStore::scheduleSave()
{
    m_saveTimer.start();
}

void NotificationStore::save() const
{
    QJsonArray array;
    for (const QList<Entry> *list : {&m_session, &m_earlier}) {
        for (const Entry &e : *list) {
            if (array.size() >= MaxEntries) {
                break;
            }
            array.append(QJsonObject{
                {QStringLiteral("key"), e.key},
                {QStringLiteral("summary"), e.summary},
                {QStringLiteral("body"), e.body},
                {QStringLiteral("app"), e.app},
                {QStringLiteral("appIcon"), e.appIcon},
                {QStringLiteral("icon"), e.icon},
                {QStringLiteral("desktopEntry"), e.desktopEntry},
                {QStringLiteral("created"), double(e.created.toMSecsSinceEpoch())},
            });
        }
    }
    QDir().mkpath(QFileInfo(storePath()).absolutePath());
    QSaveFile file(storePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
