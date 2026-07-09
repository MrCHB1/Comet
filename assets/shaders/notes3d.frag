#version 330 core

in vec2 uv;
flat in uint color;
in float noteHeight;
in vec2 fragPos;
in vec2 noteRes;
in vec2 notePos;

out vec4 fragColor;

uniform float animTime;
uniform float noteOutlineGlow;
uniform bool noteHsvShiftEnabled;
uniform vec3 noteHsvShifts;
uniform float noteHsvShiftStrength;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(a, b, u.x),
        mix(c, d, u.x),
        u.y
    );
}

vec3 rgbToHsv(vec3 rgb)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);

    vec4 p = mix(
        vec4(rgb.bg, K.wz),
        vec4(rgb.gb, K.xy),
        step(rgb.b, rgb.g)
    );

    vec4 q = mix(
        vec4(p.xyw, rgb.r),
        vec4(rgb.r, p.yzx),
        step(p.x, rgb.r)
    );

    float d = q.x - min(q.w, q.y);
    float e = 1e-10;

    return vec3(
        abs(q.z + (q.w - q.y) / (6.0 * d + e)), // H
        d / (q.x + e),                          // S
        q.x                                     // V
    );
}

vec3 hsvToRgb(vec3 hsv)
{
    vec3 rgb = clamp(
        abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0,
        0.0,
        1.0
    );

    return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

void main() {
    vec3 noteColor = vec3(
        float((color & 0xFF0000u) >> 16u),
        float((color & 0xFF00u) >> 8u),
        float(color & 0xFFu)
    ) / 255.0;

    if (noteHsvShiftEnabled)
    {
        vec3 noteColorHsv = rgbToHsv(noteColor);
        noteColorHsv += noteHsvShifts;
        noteColorHsv.g = clamp(noteColorHsv.g, 0.0f, 1.0f);
        noteColorHsv.b = clamp(noteColorHsv.b, 0.0f, 1.0f);
        vec3 noteColorShifted = hsvToRgb(noteColorHsv);
        noteColor = mix(noteColor, noteColorShifted, fragPos.y * noteHsvShiftStrength);
    }

    // brighter outlines
    float uvWidth = fwidth(uv.x);
    float uvHeight = fwidth(uv.y);
    const float outlineWidth = 2.5f;

    float outlineX = uvWidth * outlineWidth;
    float outlineY = uvHeight * outlineWidth;
    bool isOutline = false;
    if (uv.x < outlineX || 1.0 - uv.x < outlineX || uv.y < outlineY || 1.0 - uv.y < outlineY)
    {
        noteColor *= noteOutlineGlow;
        isOutline = true;
    }
    else
    {
        const float noiseScale = 150.0;
        vec2 p = (uv + hash(vec2(notePos.x, 0.0))) * noteRes * noiseScale;

        vec2 warp = vec2(
            noise(p + vec2(animTime * 0.8, 0.0)),
            noise(p + vec2(17.436, animTime * 0.8))
        );

        p += (warp - 0.5) * 1.5; 

        float n = noise(p);
        float v = mix(0.5, 1.07, n);
        noteColor *= v;
    }

    fragColor = vec4(noteColor, 1.0);
}
