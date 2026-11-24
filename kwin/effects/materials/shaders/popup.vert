#version 140

uniform mat4 modelViewProjectionMatrix;
uniform sampler2D popupUnit;
uniform bool keyOpaque;

in vec2 position;
in vec2 texcoord;

out vec2 uv;
flat out vec4 bg;

const int Grid = 6;
const float Coverage = 0.3;
const float Dominance = 0.6;

// Premultiplied background: the colour most solid grid samples share, or zero (popup drawn untouched) unless it
// dominates as in menus; keying cards or web content would punch holes where they resemble the background.
// Constant per popup, so found per vertex.
vec4 background()
{
    vec4 s[Grid * Grid];
    for (int i = 0; i < Grid * Grid; ++i) {
        s[i] = textureLod(popupUnit, (vec2(i % Grid, i / Grid) + 0.5) / float(Grid), 0.0);
    }
    vec4 best = vec4(0.0);
    int votes = 0;
    int solid = 0;
    for (int i = 0; i < Grid * Grid; ++i) {
        if (s[i].a < Coverage) {
            continue;
        }
        ++solid;
        int n = 0;
        for (int j = 0; j < Grid * Grid; ++j) {
            vec4 d = abs(s[i] - s[j]);
            n += int(max(max(d.r, d.g), max(d.b, d.a)) < 0.02);
        }
        if (n > votes) {
            votes = n;
            best = s[i];
        }
    }
    bool keyed = float(votes) >= Dominance * float(solid) && votes > 1 && (keyOpaque || best.a < 0.99);
    return keyed ? best : vec4(0.0);
}

void main(void)
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    uv = texcoord;
    bg = background();
}
