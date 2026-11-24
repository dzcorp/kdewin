#include "stylesettings.h"

#include <KSharedConfig>

StyleSettings::StyleSettings(QObject *parent)
    : QObject(parent)
    , m_config(Lumen::Config::get())
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("lumenrc"), KConfig::SimpleConfig)))
{
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this] {
        Lumen::Config::reload();
        const auto &next = Lumen::Config::get();
        const bool relevant = next.controlRadius != m_config.controlRadius || next.animations != m_config.animations
            || next.animationDuration != m_config.animationDuration;
        m_config = next;
        if (relevant) {
            Q_EMIT changed();
        }
    });
}
