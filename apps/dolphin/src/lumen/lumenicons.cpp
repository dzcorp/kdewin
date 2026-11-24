#include "lumenicons.h"

#include <KIconLoader>

#include <QHash>
#include <QIconEngine>
#include <QPainter>

namespace Lumen
{
namespace
{
constexpr int SmallSize = 16;
constexpr int LargeSize = 32;

class FullColorEngine : public QIconEngine
{
public:
    FullColorEngine(const QIcon &small, const QIcon &large)
        : m_small(small)
        , m_large(large)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        pick(rect.size()).paint(painter, rect, Qt::AlignCenter, mode, state);
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        return pick(size).pixmap(size, scale, mode, state);
    }
    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return pick(size).actualSize(size, mode, state);
    }
    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        return m_small.availableSizes(mode, state);
    }
    QString iconName() override
    {
        return m_small.name();
    }
    QString key() const override
    {
        return QStringLiteral("LumenFullColor");
    }
    QIconEngine *clone() const override
    {
        return new FullColorEngine(m_small, m_large);
    }

private:
    const QIcon &pick(const QSize &size) const
    {
        const int s = std::min(size.width(), size.height());
        return s > SmallSize && s < LargeSize ? m_large : m_small;
    }

    QIcon m_small;
    QIcon m_large;
};
}

QIcon fullColorIcon(const QIcon &icon)
{
    const QString name = icon.name();
    if (name.isEmpty()) {
        return icon;
    }
    static QHash<QString, QIcon> cache;
    static const bool watching = [] {
        QObject::connect(KIconLoader::global(), &KIconLoader::iconLoaderSettingsChanged, [] {
            cache.clear();
        });
        return true;
    }();
    Q_UNUSED(watching);
    if (const auto it = cache.constFind(name); it != cache.constEnd()) {
        return *it;
    }
    const QString path = KIconLoader::global()->iconPath(name, -LargeSize, true);
    const QIcon result = path.isEmpty() ? icon : QIcon(new FullColorEngine(QIcon::fromTheme(name), QIcon(path)));
    cache.insert(name, result);
    return result;
}
}
