#pragma once

#include <KDecoration3/DecorationButton>

#include <QPainterPath>
#include <QVariantAnimation>

namespace Lumen
{
class Decoration;

class Button : public KDecoration3::DecorationButton
{
    Q_OBJECT
public:
    Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);
    /// Plugin-loader constructor (KCM previews).
    Button(QObject *parent, const QVariantList &args);

    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent);

    void paint(QPainter *painter, const QRectF &repaintArea) override;
    qreal preferredWidth() const;

private:
    void drawGlyph(QPainter *p, const QRectF &box, const QColor &color) const;
    QPainterPath glyphShape(const QRectF &box, qreal scale) const;

    QVariantAnimation m_hover;
    qreal m_hoverValue = 0;
    mutable QPainterPath m_shape;
    mutable QRectF m_shapeBox;
    mutable qreal m_shapeScale = 0;
    mutable int m_shapeState = -1;
};
}
