#pragma once

#include "lumensettings.h"

#include <KQuickManagedConfigModule>
#include <KConfigWatcher>

#include <QSet>
#include <QTimer>

class QProcess;

/// System Settings → Lumen. Settings live in lumenrc (generated KConfigXT schema); on save they are
/// mirrored to KWin, the lock screen and the panel, and every Lumen component reloads live.
class LumenKcm : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(LumenSettings *settings READ settings CONSTANT)
    Q_PROPERTY(bool taskbarFound READ taskbarFound NOTIFY taskbarChanged)
    /// Panel, tray and clock options: centered, height, floating, autoHide, trayIcon, traySpacing, tasksSpacing,
    /// clockMode, clockDateStyle, clockHour, clockSeconds, clockBold, clockSize.
    Q_PROPERTY(QVariantMap taskbar READ taskbar NOTIFY taskbarChanged)
    Q_PROPERTY(bool notifyOverFullscreen READ notifyOverFullscreen WRITE setNotifyOverFullscreen NOTIFY notifyOverFullscreenChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(bool toolAvailable READ toolAvailable CONSTANT)

public:
    LumenKcm(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~LumenKcm() override;

    LumenSettings *settings() const
    {
        return m_settings;
    }

    bool taskbarFound() const
    {
        return m_found;
    }
    QVariantMap taskbar() const
    {
        return m_bar;
    }
    Q_INVOKABLE void setTaskbarValue(const QString &key, const QVariant &value);

    bool notifyOverFullscreen() const;
    void setNotifyOverFullscreen(bool on);

    bool busy() const;
    QString log() const
    {
        return m_log;
    }
    bool toolAvailable() const
    {
        return !m_tool.isEmpty();
    }

    void load() override;
    void save() override;
    void defaults() override;

    /// Run the lumen tool (optionally through pkexec); output streams into `log`.
    Q_INVOKABLE void run(const QStringList &args, bool asRoot = false);
    /// Open a panel widget's own settings dialog (Start, tray, clock).
    Q_INVOKABLE void configureWidget(const QString &pluginId);

Q_SIGNALS:
    void taskbarChanged();
    void busyChanged();
    void logChanged();
    void notifyOverFullscreenChanged();

protected:
    bool isSaveNeeded() const override;
    bool isDefaults() const override;

private:
    QVariantMap snapshot() const;
    QSet<QString> changedGroups() const;
    void applyKWin(const QSet<QString> &groups);
    void applyLockScreen();
    void applyMode();
    void applyFonts();
    void applyPanel();
    void loadPanel();
    QString shell(const QString &script) const;

    LumenSettings *m_settings;
    static QVariantMap defaultBar();
    bool m_found = false;
    QVariantMap m_bar;
    QVariantMap m_savedBar;
    QString m_savedMode;
    QVariantMap m_savedSnapshot;
    QProcess *m_proc = nullptr;
    QProcess *m_modeProc = nullptr;
    KConfigWatcher::Ptr m_notifyWatcher;
    QString m_log;
    QString m_tool;
    QString m_repo;
    QString m_startPage;
    QTimer m_autoSave;
};
