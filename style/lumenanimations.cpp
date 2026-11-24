#include "lumenanimations.h"
#include "lumenconfig.h"

#include <QPointF>
#include "lumentokens.h"

#include <QCoreApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QVariantAnimation>

namespace Lumen
{
Animations::Animations(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
    m_tick.setInterval(16);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        // Each frame re-registers targets that are still drawing busy indicators.
        const auto targets = std::exchange(m_busy, {});
        for (const auto &obj : targets) {
            if (obj) {
                QEvent ev(QEvent::StyleAnimationUpdate);
                QCoreApplication::sendEvent(obj, &ev);
            }
        }
        if (m_busy.isEmpty()) {
            m_tick.stop();
        }
    });
}

qreal Animations::busyPhase(const QObject *target, int periodMs)
{
    periodMs = Config::get().duration(periodMs);
    if (periodMs <= 0) {
        m_busy.clear();
        m_tick.stop();
        return 0.5;
    }
    if (target) {
        m_busy.insert(target, QPointer<QObject>(const_cast<QObject *>(target)));
        if (!m_tick.isActive()) {
            m_tick.start();
        }
    }
    return qreal(m_clock.elapsed() % periodMs) / periodMs;
}

void Animations::stop()
{
    m_tick.stop();
    m_busy.clear();
    for (const auto &entry : std::as_const(m_entries)) {
        entry.anim->stop();
        entry.anim->deleteLater();
    }
    m_entries.clear();
}

void Animations::forget(const QObject *target)
{
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it.key().first == target) {
            it.value().anim->deleteLater();
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
    m_watched.remove(target);
}

qreal Animations::value(const QObject *target, quintptr key, bool on, int durationMs)
{
    durationMs = Config::get().duration(durationMs);
    if (!target || durationMs <= 0) {
        return on ? 1.0 : 0.0;
    }
    const Key k(target, key);
    auto it = m_entries.find(k);
    if (it == m_entries.end()) {
        if (!on) {
            return 0.0;
        }
        auto *anim = new QVariantAnimation(this);
        QEasingCurve curve(QEasingCurve::BezierSpline);
        curve.addCubicBezierSegment(QPointF(T::Motion::Decel[0], T::Motion::Decel[1]), QPointF(T::Motion::Decel[2], T::Motion::Decel[3]), QPointF(1, 1));
        anim->setEasingCurve(curve);
        anim->setDuration(durationMs);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        auto *obj = const_cast<QObject *>(target);
        connect(anim, &QVariantAnimation::valueChanged, obj, [obj] {
            QEvent ev(QEvent::StyleAnimationUpdate);
            QCoreApplication::sendEvent(obj, &ev);
        });
        connect(anim, &QVariantAnimation::finished, this, [this, k] {
            auto e = m_entries.find(k);
            if (e != m_entries.end() && !e->on) {
                e->anim->deleteLater();
                m_entries.erase(e);
            }
        });
        if (!m_watched.contains(target)) {
            m_watched.insert(target, 1);
            connect(target, &QObject::destroyed, this, [this, target] {
                forget(target);
            });
        }
        m_entries.insert(k, {anim, true});
        anim->start();
        return 0.0;
    }
    Entry &e = it.value();
    const qreal current = e.anim->currentValue().toReal();
    if (e.on != on) {
        e.on = on;
        e.anim->stop();
        e.anim->setStartValue(current);
        e.anim->setEndValue(on ? 1.0 : 0.0);
        e.anim->setDuration(int(durationMs * (on ? 1.0 - current : current)) + 1);
        e.anim->start();
    }
    return e.anim->state() == QAbstractAnimation::Running ? current : (e.on ? 1.0 : 0.0);
}
}
