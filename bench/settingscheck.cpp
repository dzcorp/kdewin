#include <KConfigGroup>
#include <KCoreConfigSkeleton>
#include <KQuickConfigModule>
#include <KQuickConfigModuleLoader>
#include <KSharedConfig>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJSEngine>
#include <QMenu>
#include <QMetaProperty>
#include <QPainter>
#include <QProgressBar>
#include <QProcess>
#include <QPluginLoader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStylePlugin>
#include <QStyleOption>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "lumentokens.h"

#include <cmath>

static void require(bool ok, const QString &message)
{
    if (!ok) {
        qFatal("%s", qPrintable(message));
    }
}

static void settle(int ms = 400)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

class Desktop : public QDBusVirtualObject
{
public:
    QJSEngine js;
    QStringList calls;

    Desktop()
    {
        js.evaluate(QStringLiteral(R"js(
            var output = [];
            function print(s) { output.push(String(s)); }
            function widget(id) {
                return {plugin: id, index: 0, values: {}, reloads: 0,
                    readConfig: function(k, d) { return k in this.values ? this.values[k] : d; },
                    writeConfig: function(k, v) { this.values[k] = v; },
                    reloadConfig: function() { ++this.reloads; },
                    showConfigurationInterface: function() { this.configured = true; }};
            }
            var widgets = {};
            for (var id of ['org.lumen.start', 'org.lumen.tray', 'org.lumen.clock', 'org.lumen.tasks', 'org.lumen.spacer'])
                widgets[id] = widget(id);
            widgets['org.lumen.clock'].values.showDate = false;
            var panel = {height: 48, floating: false, hiding: 'none',
                widgets: function(id) { return id in widgets ? [widgets[id]] : []; }};
            function panels() { return [panel]; }
        )js"));
    }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        calls << message.path() + u'/' + message.member();
        if (message.member() == QLatin1String("evaluateScript")) {
            js.evaluate(QStringLiteral("output = [];"));
            const auto result = js.evaluate(message.arguments().value(0).toString());
            if (result.isError()) {
                connection.send(message.createErrorReply(QStringLiteral("org.lumen.TestError"), result.toString()));
            } else {
                connection.send(message.createReply(js.evaluate(QStringLiteral("output.join('\\n')")).toString()));
            }
        } else if (message.member() == QLatin1String("loadEffect")) {
            connection.send(message.createReply(true));
        } else {
            connection.send(message.createReply());
        }
        return true;
    }
};

static void executable(const QString &path, const QByteArray &body)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), path);
    require(file.write(body) == body.size(), path);
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

int main(int argc, char **argv)
{
    QTemporaryDir root;
    require(root.isValid(), QStringLiteral("temporary settings directory"));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(root.path() + QStringLiteral("/config")));
    qputenv("XDG_DATA_HOME", QFile::encodeName(root.path() + QStringLiteral("/data")));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(root.path() + QStringLiteral("/cache")));
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QApplication app(argc, argv);
    require(argc == 2, QStringLiteral("usage: settingscheck /absolute/path/kcm_lumen.so"));
    QDir binaryDir(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath());
    require(binaryDir.cd(QStringLiteral("../../..")), QStringLiteral("build module root"));
    QDir repository(binaryDir);
    require(repository.cd(QStringLiteral("../../..")), QStringLiteral("repository root"));
    qputenv("QML_IMPORT_PATH", QByteArray(QFile::encodeName(binaryDir.path()) + ':'
        + QFile::encodeName(repository.filePath(QStringLiteral("build/out/qml"))) + ':' + qgetenv("QML_IMPORT_PATH")));

    auto bus = QDBusConnection::sessionBus();
    require(bus.registerService(QStringLiteral("org.kde.KWin")), QStringLiteral("run under dbus-run-session; refusing the desktop bus"));
    require(bus.registerService(QStringLiteral("org.kde.plasmashell")), QStringLiteral("refusing the desktop bus"));
    Desktop desktop;
    require(bus.registerVirtualObject(QStringLiteral("/Effects"), &desktop), QStringLiteral("mock effects"));
    require(bus.registerVirtualObject(QStringLiteral("/KWin"), &desktop), QStringLiteral("mock KWin"));
    require(bus.registerVirtualObject(QStringLiteral("/PlasmaShell"), &desktop), QStringLiteral("mock Plasma"));

    const QString bin = root.path() + QStringLiteral("/bin");
    QDir().mkpath(bin);
    const QByteArray stub = "#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$LUMEN_TEST_ACTIONS\"\n";
    for (const char *name : {"lumen", "plasma-apply-lookandfeel", "pkexec"}) {
        executable(bin + u'/' + QLatin1String(name), stub);
    }
    qputenv("PATH", QByteArray(QFile::encodeName(bin) + ':' + qgetenv("PATH")));
    qputenv("LUMEN_TEST_ACTIONS", QFile::encodeName(root.path() + QStringLiteral("/actions")));
    QDir().mkpath(root.path() + QStringLiteral("/data/wallpapers/Lumen"));
    KConfig originalLock(QStringLiteral("kscreenlockerrc"), KConfig::SimpleConfig);
    KConfigGroup(&originalLock, QStringLiteral("Greeter")).writeEntry("WallpaperPlugin", QStringLiteral("org.kde.slideshow"));
    auto lockImage = originalLock.group(QStringLiteral("Greeter")).group(QStringLiteral("Wallpaper")).group(QStringLiteral("org.kde.image")).group(QStringLiteral("General"));
    lockImage.writeEntry("Image", QStringLiteral("file:///original/image.png"));
    lockImage.writeEntry("PreviewImage", QStringLiteral("file:///original/preview.png"));
    originalLock.group(QStringLiteral("Greeter")).group(QStringLiteral("LnF")).group(QStringLiteral("General")).writeEntry("hideClockWhenIdle", true);
    originalLock.sync();

    const auto result = KQuickConfigModuleLoader::loadModule(KPluginMetaData(QString::fromLocal8Bit(argv[1])));
    require(bool(result), result.errorString);
    std::unique_ptr<KQuickConfigModule> module(result.plugin);
    module->load();
    QObject *settings = module->property("settings").value<QObject *>();
    auto *skeleton = qobject_cast<KCoreConfigSkeleton *>(settings);
    require(skeleton, QStringLiteral("generated settings object"));
    require(module->property("taskbarFound").toBool(), QStringLiteral("taskbar detection"));

    QMap<QString, QVariant> changed;
    for (auto *item : skeleton->items()) {
        if (item->group() == QLatin1String("Appearance")) {
            continue;
        }
        const QByteArray name = item->name().toLatin1();
        const int index = settings->metaObject()->indexOfProperty(name.constData());
        require(index >= 0, item->name() + QStringLiteral(" missing property"));
        const auto prop = settings->metaObject()->property(index);
        require(prop.hasNotifySignal() && prop.isWritable(), item->name() + QStringLiteral(" missing write/notify"));
        QVariant next = item->property();
        if (next.metaType().id() == QMetaType::Bool) {
            next = !next.toBool();
        } else if (next.metaType().id() == QMetaType::Int) {
            next = item->maxValue();
        } else {
            next = QStringLiteral("foo, bar, BAZ");
        }
        require(prop.write(settings, next), item->name() + QStringLiteral(" write failed"));
        changed[item->name()] = prop.read(settings);
    }
    settle();
    KConfig disk(QStringLiteral("lumenrc"), KConfig::SimpleConfig);
    for (auto *item : skeleton->items()) {
        if (changed.contains(item->name())) {
            const KConfigGroup group(&disk, item->group());
            require(group.hasKey(item->key()), item->name() + QStringLiteral(" autosave key missing"));
            const QVariant actual = group.readEntry(item->key(), item->property());
            require(actual == changed.value(item->name()), item->name() + QStringLiteral(" autosave did not persist"));
        }
    }

    KConfig kwin(QStringLiteral("kwinrc"), KConfig::SimpleConfig);
    const KConfigGroup materials(&kwin, QStringLiteral("Effect-lumen_materials"));
    for (const char *key : {"AcrylicStrength", "AcrylicNoise", "AcrylicSaturation", "MicaStrength", "MicaSaturation", "MicaForPanels", "WallpaperTone"}) {
        const QByteArray name = QByteArray("materials") + key;
        const auto expected = settings->property(name.constData());
        require(materials.hasKey(key) && materials.readEntry(key, expected) == expected, QString::fromLatin1(key) + QStringLiteral(" not mirrored to KWin"));
    }
    require(KConfigGroup(&kwin, QStringLiteral("org.kde.kdecoration2")).readEntry("ButtonsOnLeft") == QLatin1String("XIA"), QStringLiteral("button layout"));
    require(KConfigGroup(&kwin, QStringLiteral("Effect-lumen_open")).readEntry("OpenScale", 0.0) == 1.0, QStringLiteral("open scale percent conversion"));
    KConfig fontGlobals(QStringLiteral("kdeglobals"), KConfig::SimpleConfig);
    const auto checkFont = [&](const char *group, const char *key, double base) {
        const QString font = fontGlobals.group(QString::fromLatin1(group)).readEntry(key);
        require(font.section(u',', 1, 1).toDouble() == std::round(base * 2.25 * 10) / 10,
                QString::fromLatin1(key) + QStringLiteral(" text size scaling"));
    };
    checkFont("General", "font", Lumen::T::Font::SizePt);
    checkFont("General", "menuFont", Lumen::T::Font::MenuPt);
    checkFont("General", "toolBarFont", Lumen::T::Font::SizePt);
    checkFont("General", "smallestReadableFont", Lumen::T::Font::SmallPt);
    checkFont("General", "fixed", Lumen::T::Font::MonoPt);
    checkFont("WM", "activeFont", Lumen::T::Font::SizePt);
    originalLock.reparseConfiguration();
    require(originalLock.group(QStringLiteral("Greeter")).readEntry("WallpaperPlugin") == QLatin1String("org.kde.image"), QStringLiteral("lock wallpaper plugin not activated"));
    require(lockImage.readEntry("Image").contains(QLatin1String("/wallpapers/Lumen")), QStringLiteral("lock wallpaper missing"));

    QQmlEngine firstEngine, secondEngine;
    firstEngine.addImportPath(binaryDir.path());
    secondEngine.addImportPath(binaryDir.path());
    const QByteArray binding = "import QtQml\nimport org.lumen.settings as Lumen\nQtObject { property int radius: Lumen.Settings.controlRadius; property real scale: Lumen.Settings.animationScale }";
    QQmlComponent firstComponent(&firstEngine), secondComponent(&secondEngine);
    firstComponent.setData(binding, QUrl());
    secondComponent.setData(binding, QUrl());
    std::unique_ptr<QObject> first(firstComponent.create()), second(secondComponent.create());
    require(first && second, firstComponent.errorString() + secondComponent.errorString());
    require(first->property("radius").toInt() == 12 && first->property("scale").toDouble() == 0, QStringLiteral("Qt Quick initial settings"));

#ifdef LUMEN_CHECK_STYLE
    QPluginLoader styleLoader(binaryDir.filePath(QStringLiteral("lumen6.so")));
    auto *stylePlugin = qobject_cast<QStylePlugin *>(styleLoader.instance());
    require(stylePlugin, styleLoader.errorString());
    QStyle *style = stylePlugin->create(QStringLiteral("Lumen"));
    require(style, QStringLiteral("load Lumen widget style"));
    app.setStyle(style);
    require(style->styleHint(QStyle::SH_Widget_Animation_Duration) == 0, QStringLiteral("widget animations were not disabled"));
    QMenu menu;
    menu.ensurePolished();
    const auto menuAlpha = [&] {
        QImage image(80, 50, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOption option;
        option.initFrom(&menu);
        option.rect = image.rect();
        style->drawPrimitive(QStyle::PE_PanelMenu, &option, &painter, &menu);
        return qAlpha(image.pixel(40, 25));
    };
    require(menuAlpha() == 255, QStringLiteral("disabled menu material remained translucent"));
    QProgressBar progress;
    QStyleOptionProgressBar progressOption;
    progressOption.initFrom(&progress);
    progressOption.rect = QRect(0, 0, 160, 8);
    progressOption.minimum = progressOption.maximum = 0;
    const auto busyImage = [&] {
        QImage image(160, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style->drawControl(QStyle::CE_ProgressBarContents, &progressOption, &painter, &progress);
        return image;
    };
    const auto pausedProgress = busyImage();
    settle(50);
    require(pausedProgress == busyImage(), QStringLiteral("busy indicator ignored disabled animations"));
#endif
    settings->setProperty("styleControlRadius", 7);
    settings->setProperty("styleAnimations", true);
    settings->setProperty("styleAnimationDuration", 200);
    settings->setProperty("styleTranslucentMenus", true);
    settings->setProperty("materialsEnabled", true);
    settle();
    disk.reparseConfiguration();
    require(first->property("radius").toInt() == 7 && second->property("radius").toInt() == 7,
            QStringLiteral("Qt Quick multi-engine radius refresh: first=%1 second=%2 property=%3 disk=%4")
                .arg(first->property("radius").toInt()).arg(second->property("radius").toInt())
                .arg(settings->property("styleControlRadius").toInt()).arg(disk.group(QStringLiteral("Style")).readEntry("ControlRadius", -1)));
    require(first->property("scale").toDouble() == 2 && second->property("scale").toDouble() == 2, QStringLiteral("Qt Quick animation refresh"));
#ifdef LUMEN_CHECK_STYLE
    require(style->styleHint(QStyle::SH_Widget_Animation_Duration) == int(Lumen::T::Motion::FastMs) * 2, QStringLiteral("widget duration refresh"));
    require(menuAlpha() < 255, QStringLiteral("existing menu did not enable translucency"));
#endif

    const auto bar = module->property("taskbar").toMap();
    require(bar.value(QStringLiteral("clockMode")).toInt() == 2, QStringLiteral("legacy clock mode not reflected"));
    for (auto it = bar.cbegin(); it != bar.cend(); ++it) {
        QVariant value = it.value().metaType().id() == QMetaType::Bool ? QVariant(!it.value().toBool())
            : QVariant(it.key().endsWith(QLatin1String("Align")) ? 0 : it.value().toInt() + 1);
        require(QMetaObject::invokeMethod(module.get(), "setTaskbarValue", Q_ARG(QString, it.key()), Q_ARG(QVariant, value)), it.key());
    }
    settle();
    disk.reparseConfiguration();
    const auto applied = module->property("taskbar").toMap();
    const auto panelValues = desktop.js.evaluate(QStringLiteral("readBar(panel)")).toVariant().toMap();
    for (auto it = applied.cbegin(); it != applied.cend(); ++it) {
        const KConfigGroup group(&disk, QStringLiteral("Taskbar"));
        require(group.hasKey(it.key()) && group.readEntry(it.key(), it.value()) == it.value(), it.key() + QStringLiteral(" taskbar persistence"));
        require(panelValues.contains(it.key()) && panelValues.value(it.key()) == it.value(), it.key() + QStringLiteral(" taskbar runtime value"));
    }
    require(desktop.js.evaluate(QStringLiteral("widgets['org.lumen.clock'].values.showDate")).toBool(), QStringLiteral("clock mode did not clear legacy flag"));

    require(module->setProperty("notifyOverFullscreen", true), QStringLiteral("notification property"));
    KConfig notifications(QStringLiteral("plasmanotifyrc"), KConfig::SimpleConfig);
    require(!KConfigGroup(&notifications, QStringLiteral("DoNotDisturb")).readEntry("WhenFullscreen", true), QStringLiteral("fullscreen inversion"));
    module->defaults();
    settle();
    require(!module->property("notifyOverFullscreen").toBool(), QStringLiteral("notification defaults"));
    QSignalSpy notificationChanges(module.get(), SIGNAL(notifyOverFullscreenChanged()));
    const auto externalNotifications = KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc"));
    externalNotifications->group(QStringLiteral("DoNotDisturb")).writeEntry("WhenFullscreen", false, KConfig::Notify);
    externalNotifications->sync();
    settle(100);
    require(notificationChanges.count() > 0 && module->property("notifyOverFullscreen").toBool(), QStringLiteral("external notification refresh"));
    module->setProperty("notifyOverFullscreen", false);
    QVariantMap defaultBar = bar;
    defaultBar.insert(QStringLiteral("clockMode"), 0);
    require(module->property("taskbar").toMap() == defaultBar, QStringLiteral("taskbar defaults"));
    originalLock.reparseConfiguration();
    require(originalLock.group(QStringLiteral("Greeter")).readEntry("WallpaperPlugin") == QLatin1String("org.kde.slideshow"), QStringLiteral("lock wallpaper plugin not restored"));
    require(lockImage.readEntry("Image") == QLatin1String("file:///original/image.png"), QStringLiteral("lock image not restored"));
    require(lockImage.readEntry("PreviewImage") == QLatin1String("file:///original/preview.png"), QStringLiteral("lock preview not restored"));
    require(!originalLock.group(QStringLiteral("Greeter")).group(QStringLiteral("LnF")).group(QStringLiteral("General")).readEntry("hideClockWhenIdle", true), QStringLiteral("always-show clock still hidden when idle"));

    settings->setProperty("lockscreenLumenWallpaper", true);
    settle();
    originalLock.reparseConfiguration();
    originalLock.group(QStringLiteral("Greeter")).writeEntry("WallpaperPlugin", QStringLiteral("org.kde.potd"));
    originalLock.sync();
    settings->setProperty("lockscreenLumenWallpaper", false);
    settle();
    originalLock.reparseConfiguration();
    require(originalLock.group(QStringLiteral("Greeter")).readEntry("WallpaperPlugin") == QLatin1String("org.kde.potd"), QStringLiteral("external lock wallpaper choice overwritten"));

    for (const char *mode : {"light", "dark", "auto"}) {
        require(settings->setProperty("appearanceMode", QString::fromLatin1(mode)), QStringLiteral("mode property"));
        settle();
    }
    KConfig globals(QStringLiteral("kdeglobals"), KConfig::SimpleConfig);
    require(KConfigGroup(&globals, QStringLiteral("KDE")).readEntry("AutomaticLookAndFeel", false), QStringLiteral("automatic mode disabled"));

    require(QFile::remove(bin + QStringLiteral("/lumen")), QStringLiteral("remove test tool"));
    require(QMetaObject::invokeMethod(module.get(), "run", Q_ARG(QStringList, QStringList{QStringLiteral("doctor")}), Q_ARG(bool, false)), QStringLiteral("run tool"));
    settle();
    require(!module->property("busy").toBool(), QStringLiteral("failed tool remains busy"));
    require(module->property("log").toString().contains(QLatin1String("Failed")), QStringLiteral("tool startup failure not reported"));

    executable(bin + QStringLiteral("/lumen"), stub);
    desktop.js.evaluate(QStringLiteral("panels = function() { return []; };"));
    module->load();
    require(!module->property("taskbarFound").toBool(), QStringLiteral("missing panel detection"));
    desktop.js.evaluate(QStringLiteral("panels = function() { return [panel]; };"));
    require(QMetaObject::invokeMethod(module.get(), "run", Q_ARG(QStringList, (QStringList{QStringLiteral("apply"), QStringLiteral("dark"), QStringLiteral("--layout")})), Q_ARG(bool, false)), QStringLiteral("layout action"));
    settle();
    require(module->property("taskbarFound").toBool(), QStringLiteral("layout action did not refresh panel state"));

    desktop.js.evaluate(QStringLiteral("var kcm = {settings: {appearanceMode: 'dark'}};"));
    int actions = 0;
    const QRegularExpression actionPattern(QStringLiteral("ToolAction\\s*\\{([\\s\\S]*?)\\}"));
    const QRegularExpression argsPattern(QStringLiteral("args:\\s*(\\[[^\\]]+\\])"));
    const QRegularExpression rootPattern(QStringLiteral("asRoot:\\s*true"));
    for (const char *page : {"Taskbar", "LockLogin", "Boot", "Maintenance"}) {
        QFile source(repository.filePath(QStringLiteral("kcm/ui/%1.qml").arg(QLatin1String(page))));
        require(source.open(QIODevice::ReadOnly), source.fileName());
        auto matches = actionPattern.globalMatch(QString::fromUtf8(source.readAll()));
        while (matches.hasNext()) {
            const QString body = matches.next().captured(1);
            const auto argumentMatch = argsPattern.match(body);
            require(argumentMatch.hasMatch(), body);
            const auto array = desktop.js.evaluate(argumentMatch.captured(1));
            require(!array.isError(), array.toString());
            QStringList arguments;
            for (const auto &value : array.toVariant().toList()) {
                arguments << value.toString();
            }
            require(QMetaObject::invokeMethod(module.get(), "run", Q_ARG(QStringList, arguments), Q_ARG(bool, rootPattern.match(body).hasMatch())), QStringLiteral("tool action invocation"));
            QElapsedTimer deadline;
            deadline.start();
            while (module->property("busy").toBool() && deadline.elapsed() < 1000) {
                settle(10);
            }
            require(module->property("log").toString().contains(QLatin1String("Done.")), arguments.join(u' '));
            ++actions;
        }
    }
    require(actions == 12, QStringLiteral("unexpected tool action coverage"));
    require(module->mainUi(), module->errorString());
    for (const char *page : {"Materials", "Windows", "Applications", "Animations", "Taskbar", "LockLogin", "Boot", "Maintenance"}) {
        module->push(QString::fromLatin1(page) + QStringLiteral(".qml"));
        settle(10);
        require(module->depth() == 2 && module->subPage(0), QString::fromLatin1(page) + QStringLiteral(" page failed to load"));
        module->pop();
    }

    settings->setProperty("styleControlRadius", 7);
    QElapsedTimer timer;
    timer.start();
    module->save();
    const double saveMs = timer.nsecsElapsed() / 1e6;
    const int settingsCount = skeleton->items().size();
    executable(bin + QStringLiteral("/plasma-apply-lookandfeel"), stub + "/bin/sleep 0.15\n");
    settings->setProperty("appearanceMode", QStringLiteral("light"));
    module->save();
    settings->setProperty("appearanceMode", QStringLiteral("dark"));
    module->save();
    settings->setProperty("appearanceMode", QStringLiteral("auto"));
    module->save();
    module.reset();
    settle(400);
    globals.reparseConfiguration();
    require(globals.group(QStringLiteral("KDE")).readEntry("AutomaticLookAndFeel", false), QStringLiteral("closing during mode change lost the latest mode"));

    for (const char *command : {"qdbus6", "systemctl", "dbus-send", "gsettings", "kbuildsycoca6"}) {
        executable(bin + u'/' + QLatin1String(command), stub);
    }
    KConfig preferences(QStringLiteral("lumenrc"), KConfig::SimpleConfig);
    preferences.group(QStringLiteral("Materials")).writeEntry("Enabled", false);
    auto motion = preferences.group(QStringLiteral("Animations"));
    motion.writeEntry("WindowOpen", false);
    motion.writeEntry("Minimize", false);
    motion.writeEntry("WindowDuration", 150);
    motion.writeEntry("OpenScale", 73);
    preferences.group(QStringLiteral("LockScreen")).writeEntry("LumenWallpaper", true);
    preferences.sync();
    globals.group(QStringLiteral("Colors:Window")).writeEntry("BackgroundNormal", QStringLiteral("255,255,255"));
    globals.sync();
    const auto reapply = [&] {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(repository.filePath(QStringLiteral("tools/lumen/target/release/lumen")),
                      {QStringLiteral("--root"), repository.path(), QStringLiteral("apply"), QStringLiteral("auto")});
        require(process.waitForFinished(20000) && process.exitCode() == 0, QString::fromLocal8Bit(process.readAll()));
    };
    reapply();
    kwin.reparseConfiguration();
    const auto plugins = kwin.group(QStringLiteral("Plugins"));
    require(!plugins.readEntry("lumen_materialsEnabled", true) && plugins.readEntry("blurEnabled", false), QStringLiteral("reapply ignored disabled materials"));
    require(!plugins.readEntry("lumen_openEnabled", true) && !plugins.readEntry("lumen_minimizeEnabled", true), QStringLiteral("reapply enabled disabled animations"));
    const auto open = kwin.group(QStringLiteral("Effect-lumen_open"));
    require(open.readEntry("OpenScale", 0.0) == 0.73, QStringLiteral("reapply lost the animation scale"));
    require(open.readEntry("OpenDuration", 0) == int(Lumen::T::Motion::NormalMs * 1.5), QStringLiteral("reapply lost the animation duration"));
    globals.reparseConfiguration();
    require(globals.group(QStringLiteral("KDE")).readEntry("AutomaticLookAndFeel", false), QStringLiteral("CLI automatic mode disabled"));
    preferences.reparseConfiguration();
    require(preferences.group(QStringLiteral("Appearance")).readEntry("Mode") == QLatin1String("auto"), QStringLiteral("CLI mode not persisted"));
    originalLock.reparseConfiguration();
    require(originalLock.group(QStringLiteral("Greeter")).readEntry("WallpaperPlugin") == QLatin1String("org.kde.image"), QStringLiteral("CLI lock wallpaper plugin"));
    preferences.group(QStringLiteral("LockScreen")).writeEntry("LumenWallpaper", false);
    preferences.sync();
    reapply();
    originalLock.reparseConfiguration();
    require(originalLock.group(QStringLiteral("Greeter")).readEntry("WallpaperPlugin") == QLatin1String("org.kde.potd"), QStringLiteral("CLI lock wallpaper restore"));
    QTextStream(stdout) << "PASS " << settingsCount << " schema settings, " << bar.size()
                        << " taskbar values, " << actions << " tool actions, Qt Quick/widget refresh, lock wallpaper restore, defaults, modes, CLI reapply and tool failures; save " << saveMs << " ms\n";
}
