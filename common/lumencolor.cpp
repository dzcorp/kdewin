#include "lumencolor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Lumen
{
namespace
{
double toLin(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double fromLin(double c)
{
    c = std::clamp(c, 0.0, 1.0);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

std::array<double, 3> oklab(const QColor &c)
{
    const double r = toLin(c.redF()), g = toLin(c.greenF()), b = toLin(c.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

QColor withLightness(const QColor &c, double L)
{
    const auto lab = oklab(c);
    double k = 1.0;
    double R = 0, G = 0, B = 0;
    for (int i = 0; i < 64; ++i, k *= 0.95) {
        const double a = lab[1] * k, b = lab[2] * k;
        const double l = std::pow(L + 0.3963377774 * a + 0.2158037573 * b, 3);
        const double m = std::pow(L - 0.1055613458 * a - 0.0638541728 * b, 3);
        const double s = std::pow(L - 0.0894841775 * a - 1.2914855480 * b, 3);
        R = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
        G = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
        B = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
        if (std::min({R, G, B}) >= -1e-4 && std::max({R, G, B}) <= 1 + 1e-4) {
            break;
        }
    }
    return QColor::fromRgbF(fromLin(R), fromLin(G), fromLin(B), c.alphaF());
}
}

qreal lightness(const QColor &c)
{
    return oklab(c)[0];
}

const Shades &accentShades(const QColor &accent)
{
    thread_local QRgb cachedKey = 0;
    thread_local Shades cached;
    const QRgb key = accent.rgba();
    if (key == cachedKey && cached.base.isValid()) {
        return cached;
    }
    cachedKey = key;
    if ((key & 0xffffffu) == (T::Accent::Default & 0xffffffu)) {
        cached = {argb(T::Accent::Light3), argb(T::Accent::Light2), argb(T::Accent::Light1), accent,
                  argb(T::Accent::Dark1), argb(T::Accent::Dark2), argb(T::Accent::Dark3)};
        return cached;
    }
    const double lb = lightness(accent);
    auto light = [&](double f) {
        return withLightness(accent, lb + (T::Accent::Lmax - lb) * f);
    };
    auto dark = [&](double f) {
        return withLightness(accent, lb * f);
    };
    cached = {light(T::Accent::LightF[2]), light(T::Accent::LightF[1]), light(T::Accent::LightF[0]), accent,
              dark(T::Accent::DarkF[0]), dark(T::Accent::DarkF[1]), dark(T::Accent::DarkF[2])};
    return cached;
}

QColor over(const QColor &top, const QColor &bottom)
{
    const qreal ta = top.alphaF(), ba = bottom.alphaF();
    const qreal a = ta + ba * (1 - ta);
    if (a <= 0) {
        return Qt::transparent;
    }
    auto ch = [&](qreal t, qreal b) {
        return (t * ta + b * ba * (1 - ta)) / a;
    };
    return QColor::fromRgbF(ch(top.redF(), bottom.redF()), ch(top.greenF(), bottom.greenF()), ch(top.blueF(), bottom.blueF()), a);
}
}
