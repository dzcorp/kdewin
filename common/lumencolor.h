#pragma once

#include "lumentokens.h"

#include <QColor>
#include <QPalette>

namespace Lumen
{
inline QColor argb(std::uint32_t v)
{
    return QColor::fromRgba(v);
}

qreal lightness(const QColor &c);

inline bool isDark(const QColor &background)
{
    return lightness(background) < 0.5;
}

inline bool isDark(const QPalette &pal)
{
    return isDark(pal.color(QPalette::Window));
}

inline const T::Fluent &fluent(bool dark)
{
    return dark ? T::Dark : T::Light;
}

inline const T::Fluent &fluent(const QPalette &pal)
{
    return fluent(isDark(pal));
}

struct Shades {
    QColor light3, light2, light1, base, dark1, dark2, dark3;
};

const Shades &accentShades(const QColor &accent);

/// Fluent accent fill: light2 on dark surfaces, dark1 on light ones.
inline QColor accentFill(const QColor &accent, bool dark)
{
    const Shades &s = accentShades(accent);
    return dark ? s.light2 : s.dark1;
}

inline QColor accentText(const QColor &accent, bool dark)
{
    const Shades &s = accentShades(accent);
    return dark ? s.light3 : s.dark2;
}

inline QColor withAlpha(QColor c, qreal a)
{
    c.setAlphaF(c.alphaF() * a);
    return c;
}

/// Source-over composite of `top` onto `bottom`.
QColor over(const QColor &top, const QColor &bottom);
}
