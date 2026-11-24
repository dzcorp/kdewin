#version 140

#include "sdf.glsl"

uniform sampler2D texUnit;
uniform mat4 colorMatrix;
uniform float offset;
uniform vec2 halfpixel;
uniform vec4 box;
uniform vec4 cornerRadius;
uniform float opacity;

// WinUI material layers over the blurred, saturated backdrop: a luminosity blend (W3C SetLum) pins the
// brightness while the backdrop keeps its hue, then the premultiplied tint. luminosity = (target luma, opacity).
uniform vec4 tint;
uniform vec2 luminosity;

vec3 setLum(vec3 c, float l)
{
    const vec3 w = vec3(0.3, 0.59, 0.11);
    c += l - dot(c, w);
    float lum = dot(c, w);
    float lo = min(min(c.r, c.g), c.b);
    float hi = max(max(c.r, c.g), c.b);
    if (lo < 0.0) {
        c = lum + (c - lum) * lum / max(lum - lo, 1e-5);
    }
    if (hi > 1.0) {
        c = lum + (c - lum) * (1.0 - lum) / max(hi - lum, 1e-5);
    }
    return c;
}

vec4 material(vec4 c)
{
    c.rgb = mix(c.rgb, setLum(c.rgb, luminosity.x) * c.a, luminosity.y);
    return c * (1.0 - tint.a) + tint;
}

in vec2 uv;
in vec2 vertex;

out vec4 fragColor;

void main(void)
{
    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;

    vec4 c = (sum / 12.0) * colorMatrix;
    fragColor = material(c) * opacity;

    float f = sdfRoundedBox(vertex, box.xy, box.zw, cornerRadius);
    float df = fwidth(f);
    fragColor *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
}
