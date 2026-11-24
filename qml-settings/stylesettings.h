#pragma once

#include "lumenconfig.h"

#include <KConfigWatcher>
#include <QObject>
#include <QtQmlIntegration>

class StyleSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON
    Q_PROPERTY(int controlRadius READ controlRadius NOTIFY changed)
    Q_PROPERTY(bool animations READ animations NOTIFY changed)
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY changed)
    Q_PROPERTY(qreal animationScale READ animationScale NOTIFY changed)

public:
    explicit StyleSettings(QObject *parent = nullptr);
    int controlRadius() const { return m_config.controlRadius; }
    bool animations() const { return m_config.animations; }
    int animationDuration() const { return m_config.animationDuration; }
    qreal animationScale() const { return m_config.animations ? m_config.animationDuration / 100.0 : 0; }

Q_SIGNALS:
    void changed();

private:
    Lumen::Config m_config;
    KConfigWatcher::Ptr m_watcher;
};
