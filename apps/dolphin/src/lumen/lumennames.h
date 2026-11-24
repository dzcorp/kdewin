#pragma once

#include <QRegularExpression>
#include <QString>
#include <QUrl>

namespace Lumen
{
/// The application's name, as Windows calls its file manager in the user's language.
QString explorerName();

/// Solid names unlabelled volumes "827.0 GiB Internal Drive (nvme0n1p6)"; File Explorer names drives without their size.
inline QString driveName(const QString &name)
{
    static const QRegularExpression size(QStringLiteral("^\\d+[.,]\\d+\\s*\\S+\\s+|\\s+\\d+[.,]\\d+\\s*\\S+$"),
                                         QRegularExpression::UseUnicodePropertiesOption);
    QString bare = name;
    bare.remove(size);
    return bare.isEmpty() ? name : bare;
}
}

/// File Explorer's This PC: the drives, served by the thispc KIO worker and shown by ThisPcPage.
namespace ThisPc
{
inline constexpr QLatin1StringView Scheme("thispc");

inline QUrl url()
{
    return QUrl(QStringLiteral("thispc:/"));
}

/// The page itself; thispc:/<drive> paths redirect to the drive.
inline bool is(const QUrl &url)
{
    return url.scheme() == Scheme && (url.path().isEmpty() || url.path() == QLatin1String("/"));
}

QString title();
QString recentFilesTitle();

/// Where File Explorer opens without a location (lumenrc [Files] OpenTo), shared by Lumen's settings and Dolphin's.
enum class OpenTo { ThisPc, Session, RecentFiles, Home };
OpenTo openTo();
void setOpenTo(OpenTo where);

inline bool restoresSession()
{
    return openTo() == OpenTo::Session;
}

/// The first window's location; a restored session replaces it.
QUrl startUrl(const QUrl &home);
}
