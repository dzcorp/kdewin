#pragma once

#include <tuple>

#include "lumencolor.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButtonGroup>

namespace Lumen
{
class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT
public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Decoration() override;

    bool init() override;
    void paint(QPainter *painter, const QRectF &repaintArea) override;

    qreal titleBarHeight() const;
    bool isDark() const
    {
        return m_dark;
    }
    qreal titleBarAlpha() const;
    bool isTranslucent() const;
    QColor titleBarColor() const;
    QColor fontColor() const;
    const T::Fluent &palette() const
    {
        return Lumen::fluent(isDark());
    }

private:
    void createButtons();
    void updateLayout();
    void updateButtonsGeometry();
    void updateTitleBarGeometry();
    void updateDark();
    void updateShadow();
    bool isEdgeToEdge() const;
    qreal snap(qreal v) const;

    KDecoration3::DecorationButtonGroup *m_left = nullptr;
    KDecoration3::DecorationButtonGroup *m_right = nullptr;
    bool m_shadowOnly = false;
    bool m_dark = true;
    std::tuple<bool, int, int, int, bool, bool> m_look;
};
}
