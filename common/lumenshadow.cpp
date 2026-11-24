#include "lumenshadow.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Lumen
{
namespace
{
/// Coverage of a gaussian-blurred half-plane at signed inside distance d (CSS blur = 2σ).
inline float coverage(float d, float blur)
{
    if (blur <= 0.f) {
        return d >= 0.f ? 1.f : 0.f;
    }
    return 0.5f * std::erfc(-d / (blur * 0.5f * float(M_SQRT2)));
}
}

ShadowImage renderShadow(std::span<const T::Layer> layers, qreal radius, int boxSize)
{
    int padL = 0, padT = 0, padR = 0, padB = 0;
    for (const auto &l : layers) {
        padL = std::max(padL, int(std::ceil(l.blur - l.x)));
        padT = std::max(padT, int(std::ceil(l.blur - l.y)));
        padR = std::max(padR, int(std::ceil(l.blur + l.x)));
        padB = std::max(padB, int(std::ceil(l.blur + l.y)));
    }
    const int box = boxSize > 0 ? boxSize : int(std::ceil(radius)) * 2 + 2;
    const int w = padL + box + padR;
    const int h = padT + box + padB;

    // Separable: the shadow of a sharp box is the product of two 1-D edge coverages.
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    std::vector<float> cx(static_cast<size_t>(w)), cy(static_cast<size_t>(h));
    std::vector<float> keep(size_t(w) * size_t(h), 1.f);
    for (const auto &l : layers) {
        const float bx0 = padL + float(l.x), by0 = padT + float(l.y);
        for (int x = 0; x < w; ++x) {
            const float px = x + 0.5f;
            cx[size_t(x)] = std::min(coverage(px - bx0, float(l.blur)), coverage(bx0 + box - px, float(l.blur)));
        }
        for (int y = 0; y < h; ++y) {
            const float py = y + 0.5f;
            cy[size_t(y)] = std::min(coverage(py - by0, float(l.blur)), coverage(by0 + box - py, float(l.blur)));
        }
        const float a = float(l.alpha);
        for (int y = 0; y < h; ++y) {
            float *row = keep.data() + size_t(y) * size_t(w);
            const float yy = a * cy[size_t(y)];
            for (int x = 0; x < w; ++x) {
                row[x] *= 1.f - yy * cx[size_t(x)];
            }
        }
    }
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const float *row = keep.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) {
            line[x] = qRgba(0, 0, 0, int(std::lround((1.f - row[x]) * 255.f)));
        }
    }

    const QRect inner(padL, padT, box, box);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    QPainterPath hole;
    hole.addRoundedRect(QRectF(inner), radius, radius);
    p.fillPath(hole, Qt::black);
    p.end();
    return {img, QMargins(padL, padT, padR, padB), inner};
}
}
