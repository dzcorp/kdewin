#include "lumenglyph.h"

#include <QGuiApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPalette>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Lumen
{
static qreal snap(qreal v, qreal scale)
{
    return std::round(v * scale) / scale;
}

QPainterPath strokeGlyph(const QPainterPath &path, qreal scale)
{
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(1 / scale, snap(1.0, scale)));
    stroker.setCapStyle(Qt::FlatCap);
    stroker.setJoinStyle(Qt::MiterJoin);
    return stroker.createStroke(path).simplified();
}

QPainterPath captionGlyph(Caption kind, const QRectF &box, qreal scale)
{
    const qreal px = 1 / scale;
    auto s = [scale](qreal v) {
        return snap(v, scale);
    };
    // Box edges run through device-pixel centers; diagonals cross on a pixel center so the X has one middle pixel.
    const qreal l = s(box.left()), t = s(box.top()), w = s(box.width());
    const QRectF b(l + px / 2, t + px / 2, w - px, w - px);
    const QPointF c(l + s(w / 2) + px / 2, t + s(w / 2) + px / 2);
    const qreal h = w / 2 - px / 2;

    QPainterPath path;
    switch (kind) {
    case Caption::Minimize:
        path.moveTo(l, c.y());
        path.lineTo(l + w, c.y());
        break;
    case Caption::Maximize:
        path.addRoundedRect(b, 1, 1);
        break;
    case Caption::Restore: {
        const qreal o = s(2);
        path.addRoundedRect(QRectF(b.left(), b.top() + o, b.width() - o, b.height() - o), 1, 1);
        path.moveTo(b.left() + o, b.top() + o - px / 2);
        path.lineTo(b.left() + o, b.top());
        path.lineTo(b.right(), b.top());
        path.lineTo(b.right(), b.bottom() - o);
        path.lineTo(b.right() - o + px / 2, b.bottom() - o);
        break;
    }
    case Caption::Close:
        path.moveTo(c.x() - h, c.y() - h);
        path.lineTo(c.x() + h, c.y() + h);
        path.moveTo(c.x() + h, c.y() - h);
        path.lineTo(c.x() - h, c.y() + h);
        break;
    }
    return strokeGlyph(path, scale);
}

QPainterPath arrowGlyph(Qt::ArrowType direction, const QRectF &box, qreal scale)
{
    const qreal px = 1 / scale;
    const qreal w = snap(std::min(box.width(), box.height()), scale);
    const QPointF c(snap(box.center().x(), scale) + px / 2, snap(box.center().y(), scale) + px / 2);
    const qreal half = snap(w / 2, scale) - px / 2;
    const qreal head = snap(w * 0.4, scale);
    QPainterPath path;
    path.moveTo(c.x() + half, c.y());
    path.lineTo(c.x() - half, c.y());
    path.moveTo(c.x() - half + head, c.y() - head);
    path.lineTo(c.x() - half, c.y());
    path.lineTo(c.x() - half + head, c.y() + head);
    const qreal angle = direction == Qt::UpArrow ? 90 : direction == Qt::RightArrow ? 180 : direction == Qt::DownArrow ? 270 : 0;
    if (angle != 0) {
        path = QTransform().translate(c.x(), c.y()).rotate(angle).translate(-c.x(), -c.y()).map(path);
    }
    return strokeGlyph(path, scale);
}

QPainterPath glyphPath(Glyph glyph, const QRectF &box, qreal scale)
{
    const qreal px = 1 / scale;
    const qreal w = snap(std::min(box.width(), box.height()), scale);
    const qreal l = snap(box.center().x() - w / 2, scale) + px / 2;
    const qreal t = snap(box.center().y() - w / 2, scale) + px / 2;
    const QPointF c(l + snap(w / 2, scale), t + snap(w / 2, scale));
    auto s = [scale](qreal v) {
        return snap(v, scale);
    };
    QPainterPath path;
    switch (glyph) {
    case Glyph::Sidebar: {
        const QRectF frame(l, t + s(w * 0.12), w - px, w - 2 * s(w * 0.12) - px);
        path.addRoundedRect(frame, 2, 2);
        path.moveTo(l + s(w * 0.36), frame.top());
        path.lineTo(l + s(w * 0.36), frame.bottom());
        break;
    }
    case Glyph::Search: {
        const qreal r = w * 0.36;
        const QPointF o(l + w * 0.6, t + w * 0.4);
        path.addEllipse(o, r, r);
        path.moveTo(o.x() - r * M_SQRT1_2, o.y() + r * M_SQRT1_2);
        path.lineTo(l, t + w - px);
        break;
    }
    case Glyph::ChevronDown:
    case Glyph::ChevronUp: {
        const qreal h = s(w * 0.4);
        const qreal v = (glyph == Glyph::ChevronDown ? 1 : -1) * h / 2;
        path.moveTo(c.x() - h, c.y() - v);
        path.lineTo(c.x(), c.y() + v);
        path.lineTo(c.x() + h, c.y() - v);
        break;
    }
    case Glyph::ChevronRight:
    case Glyph::ChevronLeft: {
        const qreal h = s(w * 0.4);
        const qreal v = (glyph == Glyph::ChevronRight ? 1 : -1) * h / 2;
        path.moveTo(c.x() - v, c.y() - h);
        path.lineTo(c.x() + v, c.y());
        path.lineTo(c.x() - v, c.y() + h);
        break;
    }
    case Glyph::Pin: {
        // Upright push pin, then tilted so the needle points to the lower left.
        const qreal x = c.x();
        path.moveTo(x - w * 0.26, t + w * 0.06);
        path.lineTo(x + w * 0.26, t + w * 0.06);
        path.moveTo(x - w * 0.16, t + w * 0.06);
        path.lineTo(x - w * 0.16, t + w * 0.36);
        path.lineTo(x - w * 0.32, t + w * 0.56);
        path.lineTo(x + w * 0.32, t + w * 0.56);
        path.lineTo(x + w * 0.16, t + w * 0.36);
        path.lineTo(x + w * 0.16, t + w * 0.06);
        path.moveTo(x, t + w * 0.56);
        path.lineTo(x, t + w);
        path = QTransform().translate(c.x(), c.y()).rotate(45).translate(-c.x(), -c.y()).map(path);
        break;
    }
    }
    return strokeGlyph(path, scale);
}

namespace
{
using PathFn = std::function<QPainterPath(const QRectF &, qreal)>;

class GlyphIconEngine : public QIconEngine
{
public:
    explicit GlyphIconEngine(PathFn path)
        : m_path(std::move(path))
    {
    }
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QPalette pal = QGuiApplication::palette();
        const QColor color = pal.color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::WindowText);
        const qreal inset = rect.width() * 0.16;
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillPath(m_path(QRectF(rect).adjusted(inset, inset, -inset, -inset), scale), color);
        painter->restore();
    }
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(), size), mode, state);
        return pm;
    }
    QIconEngine *clone() const override
    {
        return new GlyphIconEngine(m_path);
    }
    QString key() const override
    {
        return QStringLiteral("LumenGlyph");
    }

private:
    PathFn m_path;
};
}

QIcon arrowIcon(Qt::ArrowType direction)
{
    return QIcon(new GlyphIconEngine([direction](const QRectF &box, qreal scale) {
        return arrowGlyph(direction, box, scale);
    }));
}

QIcon glyphIcon(Glyph glyph)
{
    return QIcon(new GlyphIconEngine([glyph](const QRectF &box, qreal scale) {
        return glyphPath(glyph, box, scale);
    }));
}
}
