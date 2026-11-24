#include "lumenconfig.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDateTime>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

namespace Lumen
{
namespace
{
Config s_config;
QDateTime s_stamp;
bool s_loaded = false;

QString path()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/lumenrc");
}

Config read()
{
    Config c;
    const auto cfg = KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig);
    cfg->reparseConfiguration();
    const KConfigGroup d(cfg, QStringLiteral("Decoration"));
    c.translucentTitlebar = d.readEntry("TranslucentTitlebar", c.translucentTitlebar);
    c.titlebarOpacity = std::clamp(d.readEntry("TitlebarOpacity", c.titlebarOpacity), 0, 100);
    c.windowRadius = std::clamp(d.readEntry("CornerRadius", c.windowRadius), 0, 24);
    c.titlebarHeight = std::clamp(d.readEntry("TitlebarHeight", c.titlebarHeight), 20, 64);
    c.centerTitle = d.readEntry("CenterTitle", c.centerTitle);
    const KConfigGroup s(cfg, QStringLiteral("Style"));
    c.animations = s.readEntry("Animations", c.animations);
    c.animationDuration = std::clamp(s.readEntry("AnimationDuration", c.animationDuration), 25, 400);
    c.scrollbarExpanded = s.readEntry("ScrollbarExpanded", c.scrollbarExpanded);
    c.translucentMenus = s.readEntry("TranslucentMenus", c.translucentMenus);
    c.controlRadius = std::clamp(s.readEntry("ControlRadius", c.controlRadius), 0, 12);
    c.translucentWindows = s.readEntry("TranslucentWindows", c.translucentWindows);
    c.windowOpacity = std::clamp(s.readEntry("WindowOpacity", c.windowOpacity), 0, 100);
    c.opaqueApps = s.readEntry("OpaqueApps", QString::fromLatin1(T::OpaqueApps)).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &app : c.opaqueApps) {
        app = app.trimmed();
    }
    c.opaqueApps.removeAll(QString());
    c.materials = KConfigGroup(cfg, QStringLiteral("Materials")).readEntry("Enabled", c.materials);
    return c;
}

}

const Config &Config::get()
{
    if (!s_loaded) {
        reload();
    }
    return s_config;
}

bool Config::reload()
{
    const QDateTime stamp = QFileInfo(path()).lastModified();
    if (s_loaded && stamp == s_stamp) {
        return false;
    }
    s_loaded = true;
    s_stamp = stamp;
    const Config next = read();
    if (next == s_config) {
        return false;
    }
    s_config = next;
    return true;
}
}
