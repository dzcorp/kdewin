#pragma once

#include <KWindowShadow>

#include <QHash>
#include <QMargins>
#include <QObject>
#include <QPointer>

class QWidget;

namespace Lumen
{
/// Compositor-side shadows + blur-behind for popups (menus, tooltips, combo popups).
class ShadowHelper : public QObject
{
public:
    enum class Kind { Flyout, Tooltip };

    explicit ShadowHelper(QObject *parent = nullptr);
    void registerWidget(QWidget *w, Kind kind);
    void unregisterWidget(QWidget *w);
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    struct Tiles {
        QList<KWindowShadowTile::Ptr> tiles;
        QMargins padding;
    };
    const Tiles &tiles(bool dark, Kind kind);
    void install(QWidget *w);
    void updateBlur(QWidget *w);

    QHash<QWidget *, Kind> m_kinds;
    QHash<QWidget *, QPointer<KWindowShadow>> m_shadows;
    Tiles m_cache[2][2];
};
}
