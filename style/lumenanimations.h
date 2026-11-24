#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QTimer>

class QVariantAnimation;

namespace Lumen
{
/// Per-(target, key) eased transitions. Idle targets cost nothing: entries exist only while
/// animating or resting at 1.
class Animations : public QObject
{
public:
    explicit Animations(QObject *parent = nullptr);

    /// Progress in [0, 1] toward `on`. `target` is the widget or QML styleObject to repaint.
    qreal value(const QObject *target, quintptr key, bool on, int durationMs);

    /// Looping phase in [0, 1) for indeterminate indicators; keeps `target` repainting while asked.
    qreal busyPhase(const QObject *target, int periodMs);
    void stop();

private:
    struct Entry {
        QVariantAnimation *anim;
        bool on;
    };
    using Key = QPair<const QObject *, quintptr>;
    void forget(const QObject *target);

    QHash<Key, Entry> m_entries;
    QHash<const QObject *, int> m_watched;
    QHash<const QObject *, QPointer<QObject>> m_busy;
    QTimer m_tick;
    QElapsedTimer m_clock;
};
}
