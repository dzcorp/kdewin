#pragma once

#include <QIcon>
#include <QPainterPath>

namespace Lumen
{
enum class Caption { Minimize, Maximize, Restore, Close };

/// Window caption glyph as a filled outline snapped to the device pixel grid (decoration and client-drawn title rows).
QPainterPath captionGlyph(Caption kind, const QRectF &box, qreal scale);

/// Outline a centerline path: Qt's 1px pens draw diagonals as jagged pixel pairs and double-blend crossings.
QPainterPath strokeGlyph(const QPainterPath &path, qreal scale);

/// Fluent navigation arrow (stem and head) pointing in `direction`.
QPainterPath arrowGlyph(Qt::ArrowType direction, const QRectF &box, qreal scale);

enum class Glyph { Sidebar, Search, ChevronDown, ChevronUp, ChevronRight, ChevronLeft, Pin };

/// Fluent shell glyph outline (title row, address row, navigation pane).
QPainterPath glyphPath(Glyph glyph, const QRectF &box, qreal scale);

/// Theme-independent arrow icon in the palette's text color.
QIcon arrowIcon(Qt::ArrowType direction);
QIcon glyphIcon(Glyph glyph);
}
