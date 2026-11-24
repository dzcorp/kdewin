#version 140

// Popups that paint their own background: that colour (see popup.vert) is unmixed out of every pixel
// (colour-to-alpha) and replaced by acrylic; pixels well below its coverage (client-drawn shadows) pass through.

uniform sampler2D texUnit;
uniform sampler2D popupUnit;
uniform sampler2D noiseUnit;
uniform mat4 colorMatrix;
uniform float offset;
uniform vec2 halfpixel;
uniform vec4 tintDark; // the popup's own background picks the dark or light material
uniform vec4 tintLight;
uniform vec2 luminosityDark;
uniform vec2 luminosityLight;
uniform float opacity;
uniform vec2 noiseSize; // zero: no grain

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

vec4 material(vec4 c, bool dark)
{
    vec4 tint = dark ? tintDark : tintLight;
    vec2 luminosity = dark ? luminosityDark : luminosityLight;
    c.rgb = mix(c.rgb, setLum(c.rgb, luminosity.x) * c.a, luminosity.y);
    return c * (1.0 - tint.a) + tint;
}

const float Coverage = 0.3;

in vec2 uv;
flat in vec4 bg;

out vec4 fragColor;

void main(void)
{
    vec4 win = texture(popupUnit, uv);
    if (bg.a == 0.0) {
        fragColor = win * opacity;
        return;
    }

    vec3 k = bg.rgb / bg.a;
    vec3 p = win.a > 0.0 ? win.rgb / win.a : vec3(0.0);
    vec3 spread = max((p - k) / max(1.0 - k, 1e-3), (k - p) / max(k, 1e-3));
    float a = clamp(max(spread.r, max(spread.g, spread.b)), 0.0, 1.0);
    vec3 fg = p - (1.0 - a) * k; // unmixed foreground, premultiplied by a
    float body = clamp((win.a - Coverage * bg.a) / ((1.0 - Coverage) * bg.a), 0.0, 1.0);

    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    vec4 acrylic = material((sum / 12.0) * colorMatrix, dot(k, vec3(0.3, 0.59, 0.11)) < 0.5);
    if (noiseSize.x > 0.0) {
        acrylic.rgb += texture(noiseUnit, gl_FragCoord.xy / noiseSize).rrr;
    }

    vec4 over = vec4(fg + (1.0 - a) * acrylic.rgb, a + (1.0 - a) * acrylic.a);
    fragColor = mix(win, over, body) * opacity;
}
