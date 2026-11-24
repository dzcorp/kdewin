#undef TRANSLATION_DOMAIN
#define TRANSLATION_DOMAIN "lumen_dolphin"

#include "lumennames.h"

#include <KIO/WorkerBase>
#include <KLocalizedString>

#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/Predicate>
#include <Solid/StorageAccess>

#include <QCoreApplication>
#include <QEventLoop>
#include <QHash>
#include <QUrl>

#include <optional>

#include <sys/stat.h>

namespace
{
// The devices KFilePlacesModel lists, so This PC and the places agree.
constexpr char Predicate[] =
    "[[[[ StorageVolume.ignored == false AND [ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]]"
    " OR [ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"
    " OR OpticalDisc.availableContent & 'Audio' ]"
    " OR StorageAccess.ignored == false ]";

struct Drive {
    QString name; // unique path segment
    Solid::Device device;
};

QList<Drive> drives()
{
    QList<Drive> out;
    QHash<QString, int> seen;
    const QList<Solid::Device> devices = Solid::Device::listFromQuery(Solid::Predicate::fromString(QString::fromLatin1(Predicate)));
    for (const Solid::Device &device : devices) {
        if (!device.is<Solid::StorageAccess>()) {
            continue;
        }
        QString name = Lumen::driveName(device.displayName()).replace(QLatin1Char('/'), QLatin1Char('-'));
        if (const int n = seen[name]++; n > 0) {
            name += QStringLiteral(" (%1)").arg(n + 1);
        }
        out.append({name, device});
    }
    return out;
}

KIO::UDSEntry folder(const QString &name, const QString &displayName, const QString &icon)
{
    KIO::UDSEntry entry;
    entry.reserve(6);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, displayName);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0500);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, icon);
    return entry;
}

KIO::UDSEntry driveEntry(const Drive &drive)
{
    KIO::UDSEntry entry = folder(drive.name, Lumen::driveName(drive.device.displayName()), drive.device.icon());
    const auto *access = drive.device.as<Solid::StorageAccess>();
    if (access->isAccessible() && !access->filePath().isEmpty()) {
        entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, access->filePath());
        entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, QUrl::fromLocalFile(access->filePath()).toString());
    }
    return entry;
}
}

class ThisPcWorker : public KIO::WorkerBase
{
public:
    ThisPcWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase("thispc", pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        if (isRoot(url)) {
            statEntry(folder(QStringLiteral("."), i18n("This PC"), QStringLiteral("computer")));
            return KIO::WorkerResult::pass();
        }
        const auto [drive, rest] = find(url);
        if (!drive) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
        if (rest.isEmpty() && !drive->device.as<Solid::StorageAccess>()->isAccessible()) {
            statEntry(driveEntry(*drive));
            return KIO::WorkerResult::pass();
        }
        return redirect(*drive, rest);
    }

    KIO::WorkerResult listDir(const QUrl &url) override
    {
        if (isRoot(url)) {
            const QList<Drive> all = drives();
            for (const Drive &drive : all) {
                listEntry(driveEntry(drive));
            }
            return KIO::WorkerResult::pass();
        }
        const auto [drive, rest] = find(url);
        if (!drive) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
        return redirect(*drive, rest);
    }

private:
    static bool isRoot(const QUrl &url)
    {
        return url.path().isEmpty() || url.path() == QLatin1String("/");
    }

    std::pair<std::optional<Drive>, QString> find(const QUrl &url)
    {
        const QString path = url.path().mid(1);
        const QString first = path.section(QLatin1Char('/'), 0, 0);
        const QList<Drive> all = drives();
        for (const Drive &drive : all) {
            if (drive.name == first) {
                return {drive, path.mid(first.size())};
            }
        }
        return {std::nullopt, QString()};
    }

    /// Mounts the drive when needed, then hands the request to its folder.
    KIO::WorkerResult redirect(const Drive &drive, const QString &rest)
    {
        Solid::Device device = drive.device;
        auto *access = device.as<Solid::StorageAccess>();
        if (!access->isAccessible()) {
            QEventLoop loop;
            Solid::ErrorType error = Solid::NoError;
            QString message;
            QObject::connect(access, &Solid::StorageAccess::setupDone, &loop, [&](Solid::ErrorType e, const QVariant &data) {
                error = e;
                message = data.toString();
                loop.quit();
            });
            if (!access->setup()) {
                return KIO::WorkerResult::fail(KIO::ERR_CANNOT_MOUNT, drive.device.displayName());
            }
            loop.exec();
            if (error != Solid::NoError || access->filePath().isEmpty()) {
                return KIO::WorkerResult::fail(KIO::ERR_CANNOT_MOUNT, message.isEmpty() ? drive.device.displayName() : message);
            }
        }
        QUrl target = QUrl::fromLocalFile(access->filePath());
        if (!rest.isEmpty() && rest != QLatin1String("/")) {
            target.setPath(target.path() + rest);
        }
        redirection(target);
        return KIO::WorkerResult::pass();
    }
};

// Embeds the protocol description; KIO loads the worker through kdemain().
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.thispc" FILE "thispc.json")
};

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_thispc"));
    if (argc != 4) {
        return -1;
    }
    ThisPcWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "thispcworker.moc"
