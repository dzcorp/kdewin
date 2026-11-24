#undef TRANSLATION_DOMAIN
#define TRANSLATION_DOMAIN "lumen_dolphin"

#include "lumennames.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <algorithm>

QString Lumen::explorerName()
{
    return i18n("File Explorer");
}

QString ThisPc::title()
{
    return i18n("This PC");
}

QString ThisPc::recentFilesTitle()
{
    return i18n("Recent files");
}

static KConfigGroup files()
{
    return KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig)->group(QStringLiteral("Files"));
}

ThisPc::OpenTo ThisPc::openTo()
{
    return OpenTo(std::clamp(files().readEntry("OpenTo", 0), 0, int(OpenTo::Home)));
}

void ThisPc::setOpenTo(OpenTo where)
{
    KConfigGroup group = files();
    if (where == OpenTo::ThisPc) {
        group.deleteEntry("OpenTo", KConfig::Notify);
    } else {
        group.writeEntry("OpenTo", int(where), KConfig::Notify);
    }
    group.sync();
}

QUrl ThisPc::startUrl(const QUrl &home)
{
    switch (openTo()) {
    case OpenTo::RecentFiles:
        return QUrl(QStringLiteral("recentlyused:/files"));
    case OpenTo::Home:
        return home;
    default:
        return url();
    }
}
