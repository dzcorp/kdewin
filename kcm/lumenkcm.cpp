#include "lumenkcm.h"
#include "lumentaskbar.h"
#include "lumentokens.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

K_PLUGIN_CLASS_WITH_JSON(LumenKcm, "kcm_lumen.json")

using namespace Lumen;
using namespace Qt::StringLiterals;

namespace
{
void kwinCall(const QString &path, const QString &iface, const QString &method, const QVariantList &args = {})
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, iface, method);
    msg.setArguments(args);
    QDBusConnection::sessionBus().asyncCall(msg);
}
}

LumenKcm::LumenKcm(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KQuickManagedConfigModule(parent, data)
    , m_settings(new LumenSettings(this))
{
    setButtons(Default);
    registerSettings(m_settings);
    // No Apply button: every change is saved shortly after it settles (sliders emit per step).
    m_autoSave.setSingleShot(true);
    m_autoSave.setInterval(250);
    connect(&m_autoSave, &QTimer::timeout, this, &LumenKcm::save);
    // needsSave stays true between edits, so observe every property's own signal.
    const QMetaMethod schedule = m_autoSave.metaObject()->method(m_autoSave.metaObject()->indexOfSlot("start()"));
    const QMetaObject *mo = m_settings->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        if (const QMetaProperty prop = mo->property(i); prop.hasNotifySignal()) {
            connect(m_settings, prop.notifySignal(), &m_autoSave, schedule);
        }
    }
    m_notifyWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc")));
    connect(m_notifyWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("DoNotDisturb")) {
            Q_EMIT notifyOverFullscreenChanged();
            settingsChanged();
        }
    });
    static const QStringList pages = {u"Materials"_s, u"Windows"_s, u"Applications"_s, u"Animations"_s, u"Taskbar"_s, u"LockLogin"_s, u"Boot"_s, u"Maintenance"_s};
    if (!args.isEmpty() && pages.contains(args.constFirst().toString())) {
        m_startPage = args.constFirst().toString();
    }

    QFile root(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("lumen/root")));
    if (root.open(QIODevice::ReadOnly)) {
        m_repo = QString::fromUtf8(root.readAll()).trimmed();
    }
    m_tool = QStandardPaths::findExecutable(QStringLiteral("lumen"));
    const QString local = m_repo + QStringLiteral("/tools/lumen/target/release/lumen");
    if (m_tool.isEmpty() && !m_repo.isEmpty() && QFile::exists(local)) {
        m_tool = local;
    }
}

LumenKcm::~LumenKcm()
{
    if (m_autoSave.isActive()) {
        save();
    }
}

QVariantMap LumenKcm::defaultBar()
{
    QVariantMap bar;
    for (const auto &d : Taskbar::defaults) {
        bar.insert(QLatin1String(d.key), d.isBool ? QVariant(d.value != 0) : QVariant(d.value));
    }
    return bar;
}

void LumenKcm::load()
{
    KQuickManagedConfigModule::load();
    m_savedMode = m_settings->appearanceMode();
    m_savedSnapshot = snapshot();
    loadPanel();
    Q_EMIT notifyOverFullscreenChanged();
    settingsChanged();
    m_autoSave.stop();

    if (!m_startPage.isEmpty()) {
        push(std::exchange(m_startPage, {}) + QStringLiteral(".qml"));
    }
}

void LumenKcm::loadPanel()
{
    const QJsonObject o = QJsonDocument::fromJson(
                              shell(QLatin1String(Taskbar::script) + u"var p = lumenPanel(); print(JSON.stringify({found: !!p, bar: p ? readBar(p) : {}}));"_s).toUtf8())
                              .object();
    m_found = o.value(u"found"_s).toBool();
    const QJsonObject read = o.value(u"bar"_s).toObject();
    QVariantMap bar = defaultBar();
    for (auto it = bar.begin(); it != bar.end(); ++it) {
        if (const QJsonValue v = read.value(it.key()); !v.isUndefined()) {
            it.value() = it.value().typeId() == QMetaType::Bool ? QVariant(v.toBool()) : QVariant(v.toInt());
        }
    }
    m_savedBar = bar;
    m_bar = bar;
    Q_EMIT taskbarChanged();
}

void LumenKcm::save()
{
    m_autoSave.stop();
    const QSet<QString> groups = changedGroups();
    KQuickManagedConfigModule::save();
    applyKWin(groups);
    if (groups.contains(u"LockScreen"_s)) {
        applyLockScreen();
    }
    m_savedSnapshot = snapshot();
    if (groups.contains(u"Fonts"_s)) {
        applyFonts();
    }
    if (m_settings->appearanceMode() != m_savedMode) {
        applyMode();
        m_savedMode = m_settings->appearanceMode();
    }
    if (m_found && m_bar != m_savedBar) {
        applyPanel();
        m_savedBar = m_bar;
    }
    setNeedsSave(false);
}

void LumenKcm::defaults()
{
    KQuickManagedConfigModule::defaults();
    setNotifyOverFullscreen(false);
    if (m_bar != defaultBar()) {
        m_bar = defaultBar();
        Q_EMIT taskbarChanged();
        settingsChanged();
    }
    m_autoSave.start();
}

bool LumenKcm::isSaveNeeded() const
{
    return m_bar != m_savedBar;
}

bool LumenKcm::isDefaults() const
{
    return (!m_found || m_bar == defaultBar()) && !notifyOverFullscreen();
}

void LumenKcm::setTaskbarValue(const QString &key, const QVariant &value)
{
    if (!m_bar.contains(key) || m_bar.value(key) == value) {
        return;
    }
    m_bar.insert(key, value);
    Q_EMIT taskbarChanged();
    settingsChanged();
    m_autoSave.start();
}

QVariantMap LumenKcm::snapshot() const
{
    QVariantMap m;
    for (const KConfigSkeletonItem *item : m_settings->items()) {
        m.insert(item->group() + u'/' + item->key(), item->property());
    }
    return m;
}

QSet<QString> LumenKcm::changedGroups() const
{
    QSet<QString> groups;
    const QVariantMap now = snapshot();
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        if (m_savedSnapshot.value(it.key()) != it.value()) {
            groups.insert(it.key().section(u'/', 0, 0));
        }
    }
    return groups;
}

// Only touch what changed: a full KWin reconfigure redecorates every window, so it runs for button layout only.
void LumenKcm::applyKWin(const QSet<QString> &groups)
{
    const LumenSettings *s = m_settings;
    const QVariantMap before = m_savedSnapshot;
    const auto was = [&before](const char *key) {
        return before.value(QString::fromLatin1(key));
    };
    auto kwin = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    const QString effects = QStringLiteral("org.kde.kwin.Effects");
    const auto toggle = [&](const char *effect, bool on) {
        kwinCall(QStringLiteral("/Effects"), effects, on ? QStringLiteral("loadEffect") : QStringLiteral("unloadEffect"), {QString::fromLatin1(effect)});
    };
    const auto reconfigure = [&](const char *effect) {
        kwinCall(QStringLiteral("/Effects"), effects, QStringLiteral("reconfigureEffect"), {QString::fromLatin1(effect)});
    };
    KConfigGroup plugins(kwin, QStringLiteral("Plugins"));

    if (groups.contains(u"Materials"_s)) {
        const bool on = s->materialsEnabled();
        if (was("Materials/Enabled").toBool() != on) {
            plugins.writeEntry("lumen_materialsEnabled", on);
            plugins.writeEntry("blurEnabled", !on); // stock blur takes over when materials are off
        }
        KConfigGroup mat(kwin, QStringLiteral("Effect-lumen_materials"));
        mat.writeEntry("AcrylicStrength", s->materialsAcrylicStrength());
        mat.writeEntry("AcrylicNoise", s->materialsAcrylicNoise());
        mat.writeEntry("AcrylicSaturation", s->materialsAcrylicSaturation());
        mat.writeEntry("MicaStrength", s->materialsMicaStrength());
        mat.writeEntry("MicaSaturation", s->materialsMicaSaturation());
        mat.writeEntry("MicaForPanels", s->materialsMicaForPanels());
        mat.writeEntry("WallpaperTone", s->materialsWallpaperTone());
    }
    if (groups.contains(u"Animations"_s)) {
        plugins.writeEntry("lumen_openEnabled", s->animationsWindowOpen());
        plugins.writeEntry("lumen_minimizeEnabled", s->animationsMinimize());
        const double scale = s->animationsWindowDuration() / 100.0;
        KConfigGroup open(kwin, QStringLiteral("Effect-lumen_open"));
        open.writeEntry("OpenDuration", int(T::Motion::NormalMs * scale));
        open.writeEntry("CloseDuration", int(T::Motion::FastMs * scale));
        open.writeEntry("OpenScale", s->animationsOpenScale() / 100.0);
        KConfigGroup minimize(kwin, QStringLiteral("Effect-lumen_minimize"));
        minimize.writeEntry("Duration", int(T::Motion::MinimizeMs * scale));
    }
    // 0 Windows (icon left; minimize, maximize, close right), 1 close only, 2 macOS (close, minimize, maximize left)
    const bool buttons = was("Decoration/ButtonLayout") != s->decorationButtonLayout() || was("Decoration/ShowIcon") != s->decorationShowIcon();
    if (buttons) {
        const QString icon = s->decorationShowIcon() ? QStringLiteral("M") : QString();
        const int layout = s->decorationButtonLayout();
        KConfigGroup deco(kwin, QStringLiteral("org.kde.kdecoration2"));
        deco.writeEntry("ButtonsOnLeft", layout == 2 ? QStringLiteral("XIA") : icon);
        deco.writeEntry("ButtonsOnRight", layout == 2 ? icon : layout == 1 ? QStringLiteral("X") : QStringLiteral("IAX"));
    }
    kwin->sync();

    if (buttons) {
        kwinCall(QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure"));
    }
    if (groups.contains(u"Materials"_s)) {
        const bool on = s->materialsEnabled();
        if (was("Materials/Enabled").toBool() != on) {
            toggle("lumen_materials", on);
            toggle("blur", !on);
        } else {
            reconfigure("lumen_materials");
        }
    }
    if (groups.contains(u"Animations"_s)) {
        if (was("Animations/WindowOpen").toBool() != s->animationsWindowOpen()) {
            toggle("lumen_open", s->animationsWindowOpen());
        } else {
            reconfigure("lumen_open");
        }
        if (was("Animations/Minimize").toBool() != s->animationsMinimize()) {
            toggle("lumen_minimize", s->animationsMinimize());
        } else {
            reconfigure("lumen_minimize");
        }
    }
}

void LumenKcm::applyLockScreen()
{
    auto cfg = KSharedConfig::openConfig(QStringLiteral("kscreenlockerrc"));
    KConfigGroup greeter(cfg, QStringLiteral("Greeter"));
    KConfigGroup lnf = cfg->group(QStringLiteral("Greeter")).group(QStringLiteral("LnF")).group(QStringLiteral("General"));
    lnf.writeEntry("alwaysShowClock", m_settings->lockscreenAlwaysShowClock());
    if (m_settings->lockscreenAlwaysShowClock()) {
        lnf.writeEntry("hideClockWhenIdle", false);
    }
    lnf.writeEntry("showMediaControls", m_settings->lockscreenMediaControls());
    KConfigGroup wp = cfg->group(QStringLiteral("Greeter")).group(QStringLiteral("Wallpaper")).group(QStringLiteral("org.kde.image")).group(QStringLiteral("General"));
    const QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("wallpapers/Lumen"), QStandardPaths::LocateDirectory);
    KConfigGroup backup(m_settings->sharedConfig(), QStringLiteral("LockScreenWallpaper"));
    if (m_settings->lockscreenLumenWallpaper() && !dir.isEmpty()) {
        if (!backup.readEntry("Saved", false)) {
            backup.writeEntry("Saved", true);
            backup.writeEntry("PluginPresent", greeter.hasKey("WallpaperPlugin"));
            backup.writeEntry("WallpaperPlugin", greeter.readEntry("WallpaperPlugin"));
            for (const char *key : {"Image", "PreviewImage"}) {
                backup.writeEntry(QString::fromLatin1(key) + QStringLiteral("Present"), wp.hasKey(key));
                backup.writeEntry(key, wp.readEntry(key));
            }
            backup.sync();
        }
        greeter.writeEntry("WallpaperPlugin", QStringLiteral("org.kde.image"));
        const QString url = QStringLiteral("file://") + dir;
        wp.writeEntry("Image", url);
        wp.writeEntry("PreviewImage", url);
    } else if (!m_settings->lockscreenLumenWallpaper()) {
        if (wp.readEntry("Image").contains(QLatin1String("wallpapers/Lumen"))) {
            for (const char *key : {"Image", "PreviewImage"}) {
                if (backup.readEntry(QString::fromLatin1(key) + QStringLiteral("Present"), false)) {
                    wp.writeEntry(key, backup.readEntry(key));
                } else {
                    wp.deleteEntry(key);
                }
            }
            if (backup.readEntry("Saved", false) && greeter.readEntry("WallpaperPlugin", QStringLiteral("org.kde.image")) == QLatin1String("org.kde.image")) {
                if (backup.readEntry("PluginPresent", false)) {
                    greeter.writeEntry("WallpaperPlugin", backup.readEntry("WallpaperPlugin"));
                } else {
                    greeter.deleteEntry("WallpaperPlugin");
                }
            }
        }
        backup.deleteGroup();
        backup.sync();
    }
    cfg->sync();
}

// An app pinned to one Lumen variant (its own color scheme menu) would stay dark or light after a switch.
static void unpinAppSchemes()
{
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    const auto files = dir.entryList({QStringLiteral("*rc")}, QDir::Files);
    for (const QString &name : files) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly) || !f.readAll().contains("[UiSettings]")) {
            continue;
        }
        KConfig rc(dir.filePath(name), KConfig::SimpleConfig);
        KConfigGroup ui(&rc, QStringLiteral("UiSettings"));
        if (ui.readEntry("ColorScheme").startsWith(QLatin1String("Lumen"))) {
            ui.deleteEntry("ColorScheme");
            rc.sync();
        }
    }
}

namespace
{
class ModeApplier : public QProcess
{
public:
    static ModeApplier *instance()
    {
        static auto *process = new ModeApplier;
        return process;
    }

    void request(const QString &mode)
    {
        m_pending = mode;
        if (state() == NotRunning) {
            applyNext();
        }
    }

private:
    ModeApplier()
        : QProcess(QCoreApplication::instance())
    {
        connect(this, &QProcess::finished, this, [this] { applyNext(); });
        connect(this, &QProcess::errorOccurred, this, [this](ProcessError error) {
            if (error == FailedToStart && !m_pending.isEmpty()) {
                QTimer::singleShot(0, this, [this] { applyNext(); });
            }
        });
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
            if (state() != NotRunning) {
                waitForFinished(3000);
            }
            if (state() != NotRunning) {
                waitForFinished(3000);
            }
        });
    }

    void applyNext()
    {
        if (m_pending.isEmpty()) {
            return;
        }
        const QString mode = std::exchange(m_pending, {});
        auto globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
        KConfigGroup kde(globals, QStringLiteral("KDE"));
        kde.writeEntry("AutomaticLookAndFeel", mode == QLatin1String("auto"), KConfig::Notify);
        kde.writeEntry("DefaultDarkLookAndFeel", QStringLiteral("org.lumen.desktop.dark"), KConfig::Notify);
        kde.writeEntry("DefaultLightLookAndFeel", QStringLiteral("org.lumen.desktop.light"), KConfig::Notify);
        globals->sync();
        unpinAppSchemes();
        if (mode != QLatin1String("auto")) {
            const QString package = mode == QLatin1String("light") ? QStringLiteral("org.lumen.desktop.light") : QStringLiteral("org.lumen.desktop.dark");
            start(QStringLiteral("plasma-apply-lookandfeel"), {QStringLiteral("--keep-auto"), QStringLiteral("--apply"), package});
        }
    }

    QString m_pending;
};
}

void LumenKcm::applyMode()
{
    if (!m_modeProc) {
        m_modeProc = ModeApplier::instance();
        connect(m_modeProc, &QProcess::finished, this, [this](int code) {
            if (code != 0) {
                m_log += i18n("Failed to apply the color mode (exit code %1).", code) + u'\n';
                Q_EMIT logChanged();
            }
        });
        connect(m_modeProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                m_log += i18n("Failed to apply the color mode: %1", m_modeProc->errorString()) + u'\n';
                Q_EMIT logChanged();
            }
        });
    }
    ModeApplier::instance()->request(m_settings->appearanceMode());
}

// Same fonts as `lumen apply`, scaled by the text size; running apps pick them up like a Fonts module change.
void LumenKcm::applyFonts()
{
    const double scale = m_settings->fontsTextSize() / 100.0;
    const auto font = [scale](const char *family, double pt, int weight) {
        return u"%1,%2,-1,5,%3,0,0,0,0,0,0,0,0,0,0,1"_s.arg(QLatin1String(family)).arg(std::round(pt * scale * 10) / 10).arg(weight);
    };
    const auto pick = [](const char *win, const char *other) {
        return QFontDatabase::hasFamily(QLatin1String(win)) ? win : other;
    };
    using namespace Lumen::T::Font;
    auto globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup general(globals, QStringLiteral("General"));
    general.writeEntry("font", font(Ui, SizePt, 400), KConfig::Notify);
    general.writeEntry("menuFont", font(Ui, MenuPt, 400), KConfig::Notify);
    general.writeEntry("toolBarFont", font(Ui, SizePt, 400), KConfig::Notify);
    general.writeEntry("smallestReadableFont", font(UiSmall, SmallPt, 400), KConfig::Notify);
    general.writeEntry("fixed", font(pick(Win32Mono, Mono), MonoPt, 400), KConfig::Notify);
    KConfigGroup(globals, QStringLiteral("WM")).writeEntry("activeFont", font(pick(Win32, Ui), SizePt, int(TitleWeight)), KConfig::Notify);
    globals->sync();
    QDBusConnection::sessionBus().send(QDBusMessage::createSignal(QStringLiteral("/KDEPlatformTheme"), QStringLiteral("org.kde.KDEPlatformTheme"), QStringLiteral("refreshFonts")));
    kwinCall(QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure"));
}

// Only what differs from the defaults is the user's choice: `lumen apply` keeps it and resets the rest.
void LumenKcm::applyPanel()
{
    KConfigGroup chosen(m_settings->sharedConfig(), QStringLiteral("Taskbar"));
    const QVariantMap defaults = defaultBar();
    for (auto it = m_bar.cbegin(); it != m_bar.cend(); ++it) {
        if (it.value() == defaults.value(it.key())) {
            chosen.deleteEntry(it.key());
        } else {
            chosen.writeEntry(it.key(), it.value());
        }
    }
    chosen.sync();
    const QString values = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(m_bar)).toJson(QJsonDocument::Compact));
    shell(QLatin1String(Taskbar::script) + u"var p = lumenPanel(); if (p) writeBar(p, %1);"_s.arg(values));
}

QString LumenKcm::shell(const QString &script) const
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                      QStringLiteral("/PlasmaShell"),
                                                      QStringLiteral("org.kde.PlasmaShell"),
                                                      QStringLiteral("evaluateScript"));
    msg.setArguments({script});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 3000);
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() ? reply.arguments().constFirst().toString() : QString();
}

void LumenKcm::configureWidget(const QString &pluginId)
{
    shell(QLatin1String(Taskbar::script)
          + QStringLiteral("var p = lumenPanel(); if (p) { var w = p.widgets('%1'); if (w.length) w[0].showConfigurationInterface(); }").arg(pluginId));
}

// Plasma's own Do Not Disturb rule; critical notifications always show.
bool LumenKcm::notifyOverFullscreen() const
{
    return !KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"))->group(QStringLiteral("DoNotDisturb")).readEntry("WhenFullscreen", true);
}

void LumenKcm::setNotifyOverFullscreen(bool on)
{
    if (on == notifyOverFullscreen()) {
        return;
    }
    auto cfg = KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"));
    cfg->group(QStringLiteral("DoNotDisturb")).writeEntry("WhenFullscreen", !on, KConfig::Notify);
    cfg->sync();
    Q_EMIT notifyOverFullscreenChanged();
    settingsChanged();
}

bool LumenKcm::busy() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void LumenKcm::run(const QStringList &args, bool asRoot)
{
    if (m_tool.isEmpty() || busy()) {
        return;
    }
    QStringList full;
    if (!m_repo.isEmpty()) {
        full << QStringLiteral("--root") << m_repo;
    }
    full << args;
    if (asRoot) {
        full.prepend(m_tool);
    }
    m_log = QStringLiteral("$ lumen ") + args.join(QLatin1Char(' ')) + QLatin1Char('\n');
    Q_EMIT logChanged();

    delete m_proc;
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setStandardInputFile(QProcess::nullDevice());
    connect(m_proc, &QProcess::readyRead, this, [this] {
        m_log += QString::fromLocal8Bit(m_proc->readAll());
        Q_EMIT logChanged();
    });
    connect(m_proc, &QProcess::stateChanged, this, [this] { Q_EMIT busyChanged(); });
    connect(m_proc, &QProcess::errorOccurred, this, [this] {
        m_log += i18n("Failed: %1", m_proc->errorString()) + u'\n';
        Q_EMIT logChanged();
    });
    connect(m_proc, &QProcess::finished, this, [this, args](int code) {
        m_log += QString::fromLocal8Bit(m_proc->readAll());
        m_log += code == 0 ? i18n("Done.") : i18n("Failed (exit code %1).", code);
        Q_EMIT logChanged();
        if (code == 0 && !args.isEmpty() && (args.first() == QLatin1String("apply") || args.first() == QLatin1String("restart"))) {
            if (m_autoSave.isActive()) {
                save();
            }
            loadPanel();
            settingsChanged();
        }
    });
    m_proc->start(asRoot ? QStringLiteral("pkexec") : m_tool, full);
}

#include "lumenkcm.moc"
