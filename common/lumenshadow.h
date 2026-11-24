#pragma once

#include "lumentokens.h"

#include <QImage>
#include <QMargins>
#include <span>

namespace Lumen
{
struct ShadowImage {
    QImage image;
    QMargins padding;
    /// The box (window) rectangle inside `image`, in image pixels.
    QRect inner;
};

/// Fluent multi-layer shadow of a rounded box. The box interior is left transparent so
/// translucent surfaces never show their own shadow. `boxSize` 0 → minimal 9-patch box.
ShadowImage renderShadow(std::span<const T::Layer> layers, qreal radius, int boxSize = 0);
}
