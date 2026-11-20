#include <array>
#include <algorithm>
#include <random>

#include "MIDIRendererEnhanced.h"
#include "App/MIDIApp.h"
#include "App/Models.h"
#include "Utils.h"
#include "App/Dialog/DialogMacros.h"
#include "App/UI/Layout/ForumLayout.h"
#include "RenderView.h"

#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "RenderEngine/Buffers.h"
#include "RenderEngine/Shaders.h"

#include "Config/ConfigUtils.h"

namespace
{
    using namespace Config;
    using namespace Comet::UI::Layout;

    constexpr const std::array<std::tuple<const char*, float>, 4> supersampleValues
    {
        std::tuple<const char*, float>{ "None", 1.0f },
        std::tuple<const char*, float>{ "Low (1.5x)", 1.5f },
        std::tuple<const char*, float>{ "Medium (1.75x)", 1.75f },
        std::tuple<const char*, float>{ "High (2.0x)", 2.0f }
    };

    const float keyPosDiff[] = {
        0.6F, 0.4F, 0.8F, 0.2F, 1.0F, 0.6F, 0.4F, 0.675F, 0.325F, 0.8F, 0.2F, 1.0F
    };

    const GradientNoise g_vortexNoise;

    glm::vec2 SampleCurlNoise(float x, float y, float t)
    {
        const float eps = 0.05f;

        float dNdy = (g_vortexNoise.Sample(x, y + eps, t) - g_vortexNoise.Sample(x, y - eps, t)) / (2.0f * eps);
        float dNdx = (g_vortexNoise.Sample(x + eps, y, t) - g_vortexNoise.Sample(x - eps, y, t)) / (2.0f * eps);

        return glm::vec2(dNdy, -dNdx);
    }

    constexpr std::array<float, 24> CUBE_VERTICES = {
        // front face
        0.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  0.0f,  1.0f,
        0.0f,  0.0f,  1.0f,
        // back face
        0.0f,  1.0f,  0.0f,
        1.0f,  1.0f,  0.0f,
        1.0f,  0.0f,  0.0f,
        0.0f,  0.0f,  0.0f
    };

    constexpr std::array<unsigned int, 36> CUBE_INDICES = {
        0, 1, 2, 2, 3, 0, // front
        1, 5, 6, 6, 2, 1, // right
        7, 6, 5, 5, 4, 7, // back
        4, 0, 3, 3, 7, 4, // left
        3, 2, 6, 6, 7, 3, // top
        4, 5, 1, 1, 0, 4  // bottom
    };

    const std::vector<float> QUAD_VERTICES = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,
    };

    const std::vector<unsigned int> QUAD_INDICES = {
        0, 1, 3,
        1, 2, 3
    };

    // CPU mirror of the shader's rgbToHsv / hsvToRgb / applyHsvShift, so the light that pressed keys
    // throw (scatter, reflections, fog) has the same hue as the key is actually drawn with.
    glm::vec3 RgbToHsvCpu(const glm::vec3& c)
    {
        const float mx = std::max({ c.r, c.g, c.b });
        const float mn = std::min({ c.r, c.g, c.b });
        const float d = mx - mn;

        float h = 0.0f;
        if (d > 1e-10f)
        {
            if (mx == c.r)      h = std::fmod((c.g - c.b) / d, 6.0f);
            else if (mx == c.g) h = (c.b - c.r) / d + 2.0f;
            else                h = (c.r - c.g) / d + 4.0f;
            h /= 6.0f;
            if (h < 0.0f) h += 1.0f;
        }
        return glm::vec3(h, mx > 1e-10f ? d / mx : 0.0f, mx);
    }

    glm::vec3 HsvToRgbCpu(const glm::vec3& hsv)
    {
        auto f = [&](float n)
            {
                const float k = std::fmod(n + hsv.x * 6.0f, 6.0f);
                return hsv.z * (1.0f - hsv.y * std::clamp(std::min(k, 4.0f - k), 0.0f, 1.0f));
            };
        return glm::vec3(f(5.0f), f(3.0f), f(1.0f));
    }

    // 'mask' is the same factor the keyboard shader uses (LocalUnitPos.z: 0 at the back of the key, 1 at the front)
    glm::vec3 ShiftedKeyColor(const glm::vec3& rgb, const EnhancedRendererSettings& s, float mask)
    {
        if (!s.hsvShiftEnabled) return rgb;

        glm::vec3 hsv = RgbToHsvCpu(rgb) + s.hsvShifts;
        hsv.x -= std::floor(hsv.x);
        hsv.y = std::clamp(hsv.y, 0.0f, 1.0f);
        hsv.z = std::clamp(hsv.z, 0.0f, 1.0f);
        return glm::mix(rgb, HsvToRgbCpu(hsv), mask * s.hsvShiftStrength);
    }

    glm::vec3 UnpackKeyColor(uint32_t rgb)
    {
        return glm::vec3(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f);
    }

#pragma region Shaders
    constexpr const char* keyboard3dvert = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aLeft;
layout (location = 2) in float aRight;
layout (location = 3) in float aPressFactor;
layout (location = 4) in uint aMeta;

uniform mat4 projection;
uniform mat4 view;
uniform float keyboardZOffset;
uniform float keyboardHeight;

out vec3 FragPos;
out vec2 TexCoord;
out float pressFactor;
flat out uint meta;

out vec3 LocalUnitPos;
out float KeyPos;

const float keyboardThickness = 0.20f;

void main()
{
    meta = aMeta;
    bool isBlack = (aMeta & (1u << 25u)) != 0u;
    pressFactor = aPressFactor;
    
    // 1. Fix White Keys: Create a micro-gap so they don't merge into a single blob
    float rawWidth = aRight - aLeft;
    float gap = isBlack ? 0.0 : 0.001;
    float keyWidth = rawWidth - gap;
    float startX = aLeft + (gap / 2.0);

    LocalUnitPos = aPos; 
    KeyPos = aLeft;

    // 2. Scale the unit cube
    vec3 localPos = aPos;
    localPos.x *= keyWidth;
    localPos.z *= keyboardHeight * 2.2;

    // 3. Fix Dimensions: Drastically reduce height to look like standard keys
    localPos.y *= isBlack ? 0.05 : 0.0175; // Much shorter Y height
    localPos.z *= isBlack ? 0.15 : 0.25; // Depth towards the camera

    localPos.z = localPos.z * (1.0 + 0.01) - 0.01;

    // 4. Pivot at the back (Z=0)
    const float maxRotation = 0.06; 
    float angle = aPressFactor * maxRotation - maxRotation;
    
    mat4 rotation = mat4(1.0);
    rotation[1][1] = cos(angle);
    rotation[1][2] = sin(angle);
    rotation[2][1] = -sin(angle);
    rotation[2][2] = cos(angle);
   
    localPos.y *= keyboardThickness;
    vec4 rotatedPos = rotation * vec4(localPos, 1.0);

    vec3 worldPos = rotatedPos.xyz;
    worldPos.x += startX;
    
    // lower black keys a little.
    if (isBlack) {
        worldPos.y -= 0.01 * keyboardThickness; 
    }

    worldPos.z += keyboardZOffset;

    gl_Position = projection * view * vec4(worldPos, 1.0);
    FragPos = worldPos;
})";

    constexpr const char* keyboard3dfrag = R"(#version 330 core

in vec3 FragPos;
in vec2 TexCoord;
in float pressFactor;
flat in uint meta;

in vec3 LocalUnitPos;
in float KeyPos;

uniform float outlineScale;
uniform float keyGlowFactor;
uniform float keyboardBrightness;
uniform vec3 cameraPos;
uniform float animTime;
uniform bool noteHsvShiftEnabled;
uniform vec3 noteHsvShifts;
uniform float noteHsvShiftStrength;

uniform sampler2D keyGlowTex;
uniform float reflectionStrength;
uniform vec3  refSaberRadiance;
uniform float keyZ0;
uniform float keyZ1;

// light scatter (GI)
uniform sampler2D giEmitTex;     // row 0: emitter radiance, row 1: (x, y, z, area)
uniform int giEmitCount;
uniform float giStrength;
uniform float giRadius;
uniform float giSaberStrength;
uniform vec3 giSaberLambda;      // saber radiance per unit length
uniform float giSaberY;
uniform float giSaberZ;
uniform float giSaberRadius;
uniform float causticStrength;
uniform float causticSpeed;

// screen space reflections
uniform sampler2D ssrColorTex;   // keyboard only, no reflections
uniform sampler2D ssrDepthTex;
uniform mat4 viewProj;
uniform int ssrPass;             // 1 = writing the SSR source buffer, 0 = final shading
uniform int ssrSteps;            // 0 disables the trace
uniform float ssrMaxDist;
uniform float ssrThickness;
uniform float ssrStrength;
uniform float ssrNear;
uniform float ssrFar;

out vec4 fragColor;

// iterated-sine water caustics, returns bright thin web-like lines in [0, 1]
float caustic(vec2 q, float t)
{
    vec2 p = q * 6.28318 - 250.0;
    vec2 i = p;
    float c = 1.0;
    const float inten = 0.01;
    for (int n = 0; n < 4; n++)
    {
        float tt = t * (1.0 - (3.5 / float(n + 1)));
        i = p + vec2(cos(tt - i.x) + sin(tt + i.y), sin(tt - i.y) + cos(tt + i.x));
        c += 1.0 / length(vec2(p.x / (sin(i.x + tt) / inten), p.y / (cos(i.y + tt) / inten)));
    }
    c /= 4.0;
    c = 1.17 - pow(c, 1.4);
    return clamp(pow(abs(c), 8.0), 0.0, 1.0);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
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

vec3 applyHsvShift(vec3 rgb, vec3 shift)
{
    vec3 hsv = rgbToHsv(rgb);
    hsv += shift;
    hsv.x = fract(hsv.x);      // wrap hue
    hsv.y = clamp(hsv.y, 0.0, 1.0);
    hsv.z = clamp(hsv.z, 0.0, 1.0);
    return hsvToRgb(hsv);
}

const float PI = 3.14159265;

float linearizeDepth(float d)
{
    float z = d * 2.0 - 1.0;
    return 2.0 * ssrNear * ssrFar / (ssrFar + ssrNear - z * (ssrFar - ssrNear));
}

float ign(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// Screen space reflection against the keyboard-only buffer. The ray is marched in world space and every
// sample is projected into that buffer, so the hit test is a plain depth comparison. Notes, fog, particles
// and the saber are not in the buffer and can never show up here.
// returns rgb = reflected radiance, a = confidence (0 = miss)
vec4 traceScreenSpace(vec3 P, vec3 N, vec3 R)
{
    vec3 origin = P + N * 0.0004;
    float stepLen = ssrMaxDist / float(ssrSteps);
    float jitter = ign(gl_FragCoord.xy);
    float prevT = 0.0;

    for (int i = 0; i < ssrSteps; i++)
    {
        float t = (float(i) + jitter + 0.1) * stepLen;
        vec4 clip = viewProj * vec4(origin + R * t, 1.0);
        if (clip.w <= 0.0) break;

        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;

        float sd = texture(ssrDepthTex, uv).r;
        if (sd < 1.0)
        {
            float diff = clip.w - linearizeDepth(sd);   // > 0: the ray is behind the stored surface
            if (diff > 0.0 && diff < ssrThickness)
            {
                // refine the hit between the last two samples
                float lo = prevT, hi = t;
                for (int j = 0; j < 5; j++)
                {
                    float mid = 0.5 * (lo + hi);
                    vec4 c = viewProj * vec4(origin + R * mid, 1.0);
                    vec2 u = c.xy / c.w * 0.5 + 0.5;
                    if (c.w > linearizeDepth(texture(ssrDepthTex, u).r)) hi = mid; else lo = mid;
                }

                vec4 c = viewProj * vec4(origin + R * hi, 1.0);
                vec2 huv = clamp(c.xy / c.w * 0.5 + 0.5, 0.0, 1.0);

                vec2 e = min(huv, 1.0 - huv);
                float edge = smoothstep(0.0, 0.05, min(e.x, e.y));
                float dist = 1.0 - smoothstep(0.5, 1.0, hi / ssrMaxDist);
                return vec4(texture(ssrColorTex, huv).rgb, edge * dist);
            }
        }
        prevT = t;
    }
    return vec4(0.0);
}

// Direct light from every pressed key. Each pressed key is a glowing area emitter;
// the point-to-disk form factor A / (pi d^2 + A) gives the physically-based
// inverse square falloff without blowing up at d -> 0. The emitter radius grows the
// effective area, spreading the same power over a wider footprint.
vec3 keyScatter(vec3 P, vec3 N)
{
    vec3 sum = vec3(0.0);
    float extra = PI * giRadius * giRadius;
    for (int i = 0; i < giEmitCount; i++)
    {
        vec3 emit = texelFetch(giEmitTex, ivec2(i, 0), 0).rgb;
        vec4 geo = texelFetch(giEmitTex, ivec2(i, 1), 0);

        vec3 toL = geo.xyz - P;
        float d2 = dot(toL, toL);
        vec3 L = toL * inversesqrt(d2 + 1e-9);

        // wrapped lambert: the emitter has a finite size, so part of it is still
        // visible even when its centre sits at/below the surface's horizon
        const float wrap = 0.35;
        float cosR = clamp((dot(N, L) + wrap) / (1.0 + wrap), 0.0, 1.0);

        float ff = geo.w / (PI * d2 + geo.w + extra);
        sum += emit * ff * cosR;
    }
    return sum;
}

// Exact irradiance from the saber, a finite line light along x in [0, 1]
// (closed-form integral of n.l / r^3 over the line, so no sampling and no loop)
vec3 saberScatter(vec3 P, vec3 N)
{
    float dy = giSaberY - P.y;
    float dz = giSaberZ - P.z;
    float dist2 = dy * dy + dz * dz;
    float d2 = dist2 + giSaberRadius * giSaberRadius;

    float s0 = 0.0 - P.x;
    float s1 = 1.0 - P.x;
    float r0 = sqrt(s0 * s0 + d2);
    float r1 = sqrt(s1 * s1 + d2);

    float iPerp = s1 / (d2 * r1) - s0 / (d2 * r0);   // integral of 1 / r^3
    float iAlong = 1.0 / r0 - 1.0 / r1;              // integral of s / r^3

    // wrapped for the same reason as the keys: the saber has thickness
    const float wrap = 0.6;
    float cosTerm = (N.y * dy + N.z * dz + wrap * sqrt(dist2)) / (1.0 + wrap);
    float e = max(iPerp * cosTerm + N.x * iAlong, 0.0);
    return giSaberLambda * e;
}

// soft hit mask for a ray passing d2 (squared) from an object of radius r; w = cone radius at that distance
float coneHit(float d2, float r, float w)
{
    float s2 = r * r + w * w;
    return exp(-d2 / s2) * (r * r / s2);
}

// analytic reflection trace: no BVH, no extra textures
vec3 traceReflection(vec3 P, vec3 R, float rough)
{
    vec3 L = vec3(0.05, 0.06, 0.08);   // faint room fill so the enamel never goes pure black

    // softboxes above the keys, placed by where their mirror image lands on the keys
    const float BOX_H = 0.35;
    if (R.y > 0.02)
    {
        float t = (BOX_H - P.y) / R.y;
        vec3 hit = P + R * t;
        float soft = 0.015 + rough * t;
        float mag = 1.0 + BOX_H / cameraPos.y;
        float depth = keyZ1 - keyZ0;
        float zA = cameraPos.z + (keyZ0 + depth * 0.22 - cameraPos.z) * mag;
        float zB = cameraPos.z + (keyZ0 + depth * 0.80 - cameraPos.z) * mag;
        float hwA = depth * 0.07 * mag;
        float hwB = depth * 0.16 * mag;
        float mA = 1.0 - smoothstep(hwA - soft, hwA + soft, abs(hit.z - zA));
        float mB = 1.0 - smoothstep(hwB - soft, hwB + soft, abs(hit.z - zB));
        float xm = smoothstep(-0.7, -0.4, hit.x) * (1.0 - smoothstep(1.4, 1.7, hit.x));
        L += vec3(0.88, 0.94, 1.0) * (mA * 6.0 + mB * 1.6) * xm;
    }

    // saber: a line along x, intersected in the yz plane
    {
        vec2 c = vec2(giSaberY - P.y, giSaberZ - P.z);
        vec2 d = R.yz;
        float s = dot(c, d) / max(dot(d, d), 1e-6);   // R is unit length, so s is the true ray distance
        if (s > 0.0)
        {
            vec2 q = c - d * s;
            float hx = P.x + R.x * s;
            float xm = smoothstep(-0.02, 0.02, hx) * (1.0 - smoothstep(0.98, 1.02, hx));
            L += refSaberRadiance * coneHit(dot(q, q), giSaberRadius, rough * s) * xm;
        }
    }

    // glow of the note area behind the saber (only visible on surfaces tilted toward it)
    if (R.z < -0.02)
    {
        float t = ((giSaberZ - 0.03) - P.z) / R.z;
        vec3 hit = P + R * t;
        float up = smoothstep(0.0, 0.02, hit.y) * (1.0 - smoothstep(0.08, 0.25, hit.y));
        L += textureLod(keyGlowTex, vec2(clamp(hit.x, 0.0, 1.0), 0.5), 0.0).rgb * up * 1.2;
    }

    // pressed keys as glowing spheres (same emitters the scatter uses)
    for (int i = 0; i < giEmitCount; i++)
    {
        vec4 geo = texelFetch(giEmitTex, ivec2(i, 1), 0);
        vec3 c = geo.xyz - P;
        float s = dot(c, R);
        if (s <= 0.0) continue;
        float d2 = dot(c, c) - s * s;
        float r = sqrt(geo.w / PI) + giRadius * 0.5;
        float w = rough * s;
        if (d2 > 9.0 * (r * r + w * w)) continue;
        L += texelFetch(giEmitTex, ivec2(i, 0), 0).rgb * coneHit(d2, r, w);
    }
    return L;
}

void main()
{
    bool isBlack = (meta & (1u << 25u)) != 0u;
    bool isPressed = (meta & (1u << 24u)) != 0u;

    // Extract the note color assigned to this key
    vec3 noteColor = vec3(
        float((meta & 0xFF0000u) >> 16u), 
        float((meta & 0xFF00u) >> 8u),
        float(meta & 0xFFu)
    ) / 255.0f;

    if (noteHsvShiftEnabled)
    {
        float bottomHeight = 1.0;
        float bottomMask = clamp(LocalUnitPos.z / bottomHeight, 0.0, 1.0);

        vec3 shiftedColor = applyHsvShift(noteColor, noteHsvShifts);
        noteColor = mix(noteColor, shiftedColor, bottomMask * noteHsvShiftStrength);
    }

    if (isPressed)
    {
        vec2 cq = LocalUnitPos.xz * vec2(0.25, 0.75) + vec2(hash(vec2(KeyPos, 0.0f)) * 13.0f, 0.0f);
        float t = animTime * causticSpeed + 23.0;
        float cs = (caustic(cq, t) + 0.5 * caustic(cq * 1.7 + 11.3, t * 1.3 + 5.0)) * causticStrength;

        vec3 surfaceColor = noteColor * (0.9f + cs * 2.2f) + vec3(cs * 0.12f);

        float uvWidth = fwidth(LocalUnitPos.x);
        float uvHeight = fwidth(LocalUnitPos.z);
        float outlinePx = 2.5f * outlineScale;

        bool isOutline = (
            LocalUnitPos.x < (uvWidth * outlinePx) || (1.0f - LocalUnitPos.x) < (uvWidth * outlinePx) || 
            LocalUnitPos.z < (uvHeight * outlinePx) || (1.0f - LocalUnitPos.z) < (uvHeight * outlinePx)
        );

        if (isOutline) {
            surfaceColor = noteColor * keyGlowFactor * 1.2f; 
        }

        fragColor = vec4(surfaceColor, 1.0f);
    }
    else
    {
        vec3 baseColor = isBlack ? vec3(0.05) : vec3(0.95);
        vec3 viewDir = normalize(cameraPos - FragPos);

        vec3 normal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
        if (dot(normal, viewDir) < 0.0) normal = -normal;

        vec3 lightDir = normalize(vec3(0.0, 0.9, 0.3));
        float diffuse = max(dot(normal, lightDir), 0.0);
        if (diffuse < 0.001) diffuse = max(dot(-normal, lightDir), 0.0);
        float ambient = 0.75;

        // enamel: diffuse body under a clearcoat
        float NdV = max(dot(normal, viewDir), 0.0);
        float rough = isBlack ? 0.03 : 0.07;
        float F0 = isBlack ? 0.055 : 0.045;
        float fres = F0 + (1.0 - F0) * pow(1.0 - NdV, 5.0);

        vec3 R = reflect(-viewDir, normal);

        // light scatter, unchanged: pressed keys and the saber light up the surrounding keys
        vec3 albedo = isBlack ? vec3(0.3) : vec3(0.9);
        vec3 scatter = keyScatter(FragPos, normal) * giStrength
                     + saberScatter(FragPos, normal) * giSaberStrength;

        vec3 body = baseColor * (ambient + diffuse * 0.75) + albedo * scatter;

        // SSR source buffer: just the lit enamel body, one bounce only (no reflections of reflections)
        if (ssrPass == 1)
        {
            // already multiplied by keyboardBrightness so that what a key reflects is exactly what the neighbour shows
            // on screen (pressed keys are written at full glow, brightness only dims the unpressed enamel)
            fragColor = vec4(body * (1.0 - fres) * keyboardBrightness, 1.0);
            return;
        }

        // analytic environment (softboxes, saber, ...) is the fallback, SSR replaces it wherever it finds a key
        // keyboardBrightness dims the enamel and the environment, but not what the keys reflect from each other:
        // a pressed key keeps glowing at full strength however dark the rest of the keyboard is
        vec3 refl = traceReflection(FragPos, R, rough) * keyboardBrightness;
        if (ssrSteps > 0)
        {
            vec4 ssr = traceScreenSpace(FragPos, normal, R);
            refl = mix(refl, ssr.rgb * ssrStrength, clamp(ssr.a, 0.0, 1.0));
        }

        vec3 lit = body * (1.0 - fres) * keyboardBrightness + refl * fres * reflectionStrength;

        fragColor = vec4(lit, 1.0f);
    }
})";

    constexpr const char* mist3dvert = R"(#version 330 core
layout (location = 0) in vec2 aPos;

uniform float fogBottom;   // bottom edge of the fog area in screen space (0..1)
uniform float fogQuadH;    // height of the fog area in screen space (0..1)

out vec2 texcoord;
void main()
{
	texcoord = aPos;
	gl_Position = vec4(aPos.x * 2.0f - 1.0f, (fogBottom + aPos.y * fogQuadH) * 2.0f - 1.0f, 0.0f, 1.0f);
})";

    // raymarched volumetric fog!!
    constexpr const char* mist3dfrag = R"(#version 330

out vec4 fragColor;
in vec2 texcoord;

uniform sampler3D fogNoise;   // tileable noise: r = density, gba = warp
uniform sampler2D fogLight;   // mip-mapped 1D strip of pressed key light along x

uniform vec3 mistColor;
uniform float mistOpacity;    // density
uniform float mistSpeed;
uniform float mistScale;
uniform float animTime;

uniform float fogYScale;      // height of the fog area in world units (keyboard width = 1)
uniform float fogDepth;       // thickness of the slab
uniform int fogSteps;
uniform float fogLightStrength;
uniform float fogLightReach;  // distance at which a key's light has fallen to half
uniform float fogAmbient;
uniform vec3 fogSaberLambda;  // saber radiance per unit length
uniform float fogSaberRadius;
uniform float fogSaberY;      // saber height above the playing surface
uniform float fogKeyY;        // pressed key light height above the playing surface
uniform float fogKeyZ;        // how far below the saber line (on the keyboard) the key lights sit
uniform float fogSigmaMin;
uniform float fogSigmaLogRatio;
uniform float fogLightRows;

float hg(float c, float g)   // 4*pi * Henyey-Greenstein, so isotropic = 1
{
    float d = 1.0 + g * g - 2.0 * g * c;
    return (1.0 - g * g) / pow(d, 1.5);
}

// interleaved gradient noise, hides the banding of the low step count
float ign(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main()
{
    const float EXTINCTION = 10.0;

    float x = texcoord.x;
    float s = texcoord.y * fogYScale;

    float fade = 1.0 - texcoord.y;
    fade *= fade;

    float freq = mistScale * 0.12;
    vec3 vel = vec3(0.12, 1.0, 0.08) * (mistSpeed * 0.15);   // drifts up the screen and a little sideways
    float stepLen = fogDepth / float(fogSteps);
    float jitter = ign(gl_FragCoord.xy);
    float reach2 = max(fogLightReach * fogLightReach, 1e-5);

    vec3 acc = vec3(0.0);
    float T = 1.0;

    for (int i = 0; i < fogSteps; i++)
    {
        // front to back: start at the camera side of the slab
        float h = fogDepth * (1.0 - (float(i) + jitter) / float(fogSteps));

        // density: a warped, two scale noise, thresholded so it breaks up into puffs and wisps
        vec3 q = (vec3(x, s, h) - vel * animTime) * freq;
        vec4 lo = texture(fogNoise, q);
        vec3 qd = q * 2.3 + (lo.gba - 0.5) * 0.6;
        float n = lo.r * 0.65 + texture(fogNoise, qd).r * 0.35;
        float dens = smoothstep(0.4, 0.82, n) * fade * mistOpacity;
        if (dens < 0.003) continue;

        float dKey = length(vec2(s + fogKeyZ, h - fogKeyY));
        float dSab = length(vec2(s, h - fogSaberY));

        // pressed keys: one fetch, the mip level grows with distance so the glow widens and softens
        // blur width grows with distance (world units, x spans 0..1); pick it from the blur stack
        float sigma = 0.004 + 0.5 * dKey;
        float v = clamp(log(sigma / fogSigmaMin) / fogSigmaLogRatio, 0.0, 1.0);
        v = (v * (fogLightRows - 1.0) + 0.5) / fogLightRows;
        vec3 keyL = textureLod(fogLight, vec2(x, v), 0.0).rgb / (1.0 + dKey * dKey / reach2);

        // light travelling toward the camera scatters a bit more
        vec2 tk = vec2(-(s + fogKeyZ), fogKeyY - h);
        vec2 ts = vec2(-s, fogSaberY - h);
        float phK = hg(-tk.y / max(length(tk), 1e-4), 0.0625);
        float phS = hg(-ts.y / max(length(ts), 1e-4), 0.0625);

        // saber: finite line light, closed form integral of 1 / r^2 along its length
        float dd = sqrt(dSab * dSab + fogSaberRadius * fogSaberRadius);
        float ang = atan((1.0 - x) / dd) + atan(x / dd);
        vec3 sab = fogSaberLambda * (ang / dd);

        // light is dimmed by the fog it travels through on its way here
        float dNear = min(dKey, dSab);
        vec3 L = (keyL * phK + sab * phS) * fogLightStrength * exp(-dens * dNear * 4.0)
            + mistColor * fogAmbient;

        float a = 1.0 - exp(-dens * EXTINCTION * stepLen);
        acc += T * a * L;
        T *= 1.0 - a;
        if (T < 0.02) break;
    }

    // premultiplied: rgb is the light scattered towards the camera, alpha is how much of the notes behind it is hidden
    fragColor = vec4(acc, 1.0 - T);
})";

    constexpr const char* notes3dvert = R"(#version 330

layout (location = 0) in vec2 aPos;

// instance data
layout (location = 1) in float nLeft;
layout (location = 2) in float nRight;
layout (location = 3) in float nStart;
layout (location = 4) in float nEnd;
layout (location = 5) in uint nColor;

out vec2 uv;
flat out uint color;
out float noteHeight;
out vec2 fragPos;
out vec2 noteRes;
out vec2 notePos;

uniform float kbHeight;

void main()
{
    float originalStart = nStart;
    float originalEnd = nEnd;

    float visibleStart = max(originalStart, 0.0);
    float visibleEnd = originalEnd;

    // Remap the visible geometry back into the original UV range.
    float uvStart = (visibleStart - originalStart) / (originalEnd - originalStart);
    float uvEnd = (visibleEnd - originalStart) / (originalEnd - originalStart);

    float x = mix(nLeft, nRight, aPos.x);
    float y = mix(visibleStart, visibleEnd, aPos.y);

    float pY = y * (1.0 - kbHeight) + kbHeight;

    gl_Position = vec4(
        x * 2.0 - 1.0,
        pY * 2.0 - 1.0,
        0.0,
        1.0
    );

    uv = vec2(aPos.x, mix(uvStart, uvEnd, aPos.y));
    fragPos = vec2(x, mix(originalStart, originalEnd, uv.y) * (1.0 - kbHeight) + kbHeight);
	
	color = nColor;
	noteRes = vec2(nRight - nLeft, nEnd - nStart);
	notePos = vec2(nLeft, nStart);
	noteHeight = nEnd - nStart;
})";

    constexpr const char* notes3dfrag = R"(#version 330

in vec2 uv;
flat in uint color;
in float noteHeight;
in vec2 fragPos;
in vec2 noteRes;
in vec2 notePos;

out vec4 fragColor;

uniform float animTime;
uniform float noteOutlineGlow;
uniform float outlineScale;
uniform bool noteHsvShiftEnabled;
uniform vec3 noteHsvShifts;
uniform float noteHsvShiftStrength;
uniform float causticStrength;
uniform float causticScale;
uniform float causticSpeed;

// iterated-sine water caustics, returns bright thin web-like lines in [0, 1]
float caustic(vec2 q, float t)
{
    vec2 p = q * 6.28318 - 250.0;
    vec2 i = p;
    float c = 1.0;
    const float inten = 0.01;
    for (int n = 0; n < 4; n++)
    {
        float tt = t * (1.0 - (3.5 / float(n + 1)));
        i = p + vec2(cos(tt - i.x) + sin(tt + i.y), sin(tt - i.y) + cos(tt + i.x));
        c += 1.0 / length(vec2(p.x / (sin(i.x + tt) / inten), p.y / (cos(i.y + tt) / inten)));
    }
    c /= 4.0;
    c = 1.17 - pow(c, 1.4);
    return clamp(pow(abs(c), 8.0), 0.0, 1.0);
}

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
    float outlineWidth = 2.5f * outlineScale;

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
        // two layers of drifting caustics, anchored to the note so they ride along with it
        vec2 seed = vec2(hash(vec2(notePos.x, 0.0)), hash(vec2(notePos.x, 1.0))) * 20.0;
        vec2 q = uv * noteRes * causticScale + seed;
        float t = animTime * causticSpeed + 23.0;

        float c1 = caustic(q, t);
        float c2 = caustic(q * 1.7 + 11.3, t * 1.3 + 5.0);
        float cs = (c1 + 0.5 * c2) * causticStrength;

        // soft inner glow hugging the outline, then a slightly dimmer body so the caustic lines pop
        float dx = min(uv.x, 1.0 - uv.x) / max(uvWidth, 1e-6);
        float dy = min(uv.y, 1.0 - uv.y) / max(uvHeight, 1e-6);
        float rim = exp(-min(dx, dy) * 0.35);

        noteColor *= 0.7 + rim * 0.6;
        noteColor += noteColor * cs * 2.0 + vec3(cs * 0.1);
    }

    fragColor = vec4(noteColor, 1.0);
})";

    constexpr const char* particle3dvert = R"(#version 330 core
layout (location = 0) in vec2 aPos;      
layout (location = 1) in vec3 iPos;      
layout (location = 2) in vec4 iColor;    
layout (location = 3) in float iScale;   

out vec4 particleColor;
out vec2 uv;

void main()
{
    particleColor = iColor;
    uv = aPos;

    vec2 centered = aPos - vec2(0.5, 0.5);
    vec2 particlePos = iPos.xy + centered * iScale;

    gl_Position = vec4(particlePos * 2.0 - 1.0, 0.0, 1.0);
})";

    inline constexpr const char* particle3dfrag = R"(#version 330 core

in vec2 uv;
in vec4 particleColor;
out vec4 fragColor;

uniform float aspect;

void main()
{
	vec2 p = uv * 2.0 - 1.0;
	p.x *= aspect;
	float dist = length(p);
	float alpha = 1.0 - smoothstep(0.95, 1.0, dist);
	fragColor = vec4(particleColor.rgb, particleColor.a * alpha);
})";

    constexpr const char* saber3dvert = R"(#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
})";

    constexpr const char* saber3dfrag = R"(#version 330 core
out vec4 FragColor;

uniform vec3 saberColor;

void main() {
    FragColor = vec4(saberColor, 1.0);
})";

    constexpr const char* flareVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in float aLeft;
layout (location = 2) in float aRight;
layout (location = 3) in uint aColor;
layout (location = 4) in float aAlpha;

uniform float kbHeight;
uniform float flareHeight;

flat out uint flareColor;
flat out float flareAlpha;
out vec2 uv;

void main()
{
    float x = mix(aLeft, aRight, aPos.x);
	float y = flareHeight * aPos.y + kbHeight;

    uv = aPos;
    flareColor = aColor;
    flareAlpha = aAlpha;

    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);
})";

    constexpr const char* flareFrag = R"(#version 330 core
flat in uint flareColor;
flat in float flareAlpha;
in vec2 uv;

out vec4 fragColor;

uniform float flareBrightness;

void main()
{
     vec3 fColor = vec3(
        float((flareColor & 0xFF0000u) >> 16u),
        float((flareColor & 0xFF00u) >> 8u),
        float(flareColor & 0xFFu)
    ) / 255.0;

    float vertical = 1.0 - smoothstep(0.0, 1.0, uv.y);
    
    float horizontal = smoothstep(0.0, 0.3, uv.x) * smoothstep(0.0, 0.3, 1.0 - uv.x);
    float alpha = vertical * horizontal * flareAlpha;

    fragColor = vec4(fColor * flareBrightness, alpha);
})";

    constexpr const char* fullscreenvert = R"(#version 330 core

layout (location = 0) in vec2 aPos;       
layout (location = 1) in vec2 aTexCoords; 

out vec2 texcoord;

void main()
{
    texcoord = aTexCoords;
    gl_Position = vec4(aPos.x * 2.0 - 1.0, aPos.y * 2.0 - 1.0, 0.0, 1.0); 
})";

    constexpr const char* downsamplefrag = R"(#version 330 core
out vec4 FragColor;
in vec2 texcoord;

uniform sampler2D srcTexture;
uniform int mipLevel;

vec3 Prefilter(vec3 color)
{
    if (mipLevel == 0) return max(color - vec3(1.0), vec3(0.0));
    return color;
}

void main()
{
    vec2 srcTexelSize = 1.0 / vec2(textureSize(srcTexture, 0));
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // A 13-tap bilinear downsample
    vec3 a = Prefilter(texture(srcTexture, vec2(texcoord.x - 2.0*x, texcoord.y + 2.0*y)).rgb);
    vec3 b = Prefilter(texture(srcTexture, vec2(texcoord.x,         texcoord.y + 2.0*y)).rgb);
    vec3 c = Prefilter(texture(srcTexture, vec2(texcoord.x + 2.0*x, texcoord.y + 2.0*y)).rgb);
    vec3 d = Prefilter(texture(srcTexture, vec2(texcoord.x - 2.0*x, texcoord.y)).rgb);
    vec3 e = Prefilter(texture(srcTexture, vec2(texcoord.x,         texcoord.y)).rgb);
    vec3 f = Prefilter(texture(srcTexture, vec2(texcoord.x + 2.0*x, texcoord.y)).rgb);
    vec3 g = Prefilter(texture(srcTexture, vec2(texcoord.x - 2.0*x, texcoord.y - 2.0*y)).rgb);
    vec3 h = Prefilter(texture(srcTexture, vec2(texcoord.x,         texcoord.y - 2.0*y)).rgb);
    vec3 i = Prefilter(texture(srcTexture, vec2(texcoord.x + 2.0*x, texcoord.y - 2.0*y)).rgb);
    vec3 j = Prefilter(texture(srcTexture, vec2(texcoord.x - x, texcoord.y + y)).rgb);
    vec3 k = Prefilter(texture(srcTexture, vec2(texcoord.x + x, texcoord.y + y)).rgb);
    vec3 l = Prefilter(texture(srcTexture, vec2(texcoord.x - x, texcoord.y - y)).rgb);
    vec3 m = Prefilter(texture(srcTexture, vec2(texcoord.x + x, texcoord.y - y)).rgb);

    vec3 downsample = e*0.125;
    downsample += (a+c+g+i)*0.03125;
    downsample += (b+d+f+h)*0.0625;
    downsample += (j+k+l+m)*0.125;


    FragColor = vec4(downsample, 1.0);
})";

    constexpr const char* upsamplefrag = R"(#version 330 core
out vec4 FragColor;
in vec2 texcoord;

uniform sampler2D srcTexture;
uniform float filterRadius;

void main()
{
    // Aspect-correct texel size
    vec2 texelSize = 1.0 / vec2(textureSize(srcTexture, 0));
    
    // Scale the texel size by your radius
    float x = filterRadius * texelSize.x;
    float y = filterRadius * texelSize.y;

    vec3 a = texture(srcTexture, vec2(texcoord.x - x, texcoord.y + y)).rgb;
    vec3 b = texture(srcTexture, vec2(texcoord.x,     texcoord.y + y)).rgb;
    vec3 c = texture(srcTexture, vec2(texcoord.x + x, texcoord.y + y)).rgb;
    
    vec3 d = texture(srcTexture, vec2(texcoord.x - x, texcoord.y)).rgb;
    vec3 e = texture(srcTexture, vec2(texcoord.x,     texcoord.y)).rgb;
    vec3 f = texture(srcTexture, vec2(texcoord.x + x, texcoord.y)).rgb;
    
    vec3 g = texture(srcTexture, vec2(texcoord.x - x, texcoord.y - y)).rgb;
    vec3 h = texture(srcTexture, vec2(texcoord.x,     texcoord.y - y)).rgb;
    vec3 i = texture(srcTexture, vec2(texcoord.x + x, texcoord.y - y)).rgb;

    vec3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;

    FragColor = vec4(upsample, 1.0);
})";

    constexpr const char* compositefrag = R"(#version 330 core
out vec4 FragColor;
in vec2 texcoord;

uniform sampler2D sceneTex;
uniform sampler2D bloomTex;
uniform float exposure;

// ACES tone mapping curve
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(sceneTex, texcoord).rgb;      
    vec3 bloomColor = texture(bloomTex, texcoord).rgb;
    
    // Add bloom to the scene
    hdrColor += bloomColor; 
    
    // Apply exposure
    hdrColor *= exposure;

    // Tone mapping
    vec3 mapped = ACESFilm(hdrColor);
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    // Dither: wide, shallow gradients (large key light size) only span a handful of 8-bit levels and show up
    // as contour bands. Triangular-PDF noise of +-1 LSB breaks them up without being visible as grain.
    float n0 = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float n1 = fract(52.9829189 * fract(dot(gl_FragCoord.xy + vec2(47.0, 17.0), vec2(0.06711056, 0.00583715))));
    mapped += (n0 + n1 - 1.0) / 255.0;

    FragColor = vec4(mapped, 1.0);
})";

    constexpr const char* resolvefrag = R"(#version 330 core
out vec4 fragColor;
uniform sampler2D srcTexture;
uniform vec2 scale;                       // render size / output size
void main()
{
    vec2 p  = floor(gl_FragCoord.xy);
    vec2 lo = p * scale, hi = (p + 1.0) * scale;
    vec3 sum = vec3(0.0); float wsum = 0.0;
    for (int y = int(floor(lo.y)); y < int(ceil(hi.y)); y++)
    for (int x = int(floor(lo.x)); x < int(ceil(hi.x)); x++)
    {
        float w = (min(hi.x, x + 1.0) - max(lo.x, float(x)))
                * (min(hi.y, y + 1.0) - max(lo.y, float(y)));      // exact area overlap
        vec3 c = texelFetch(srcTexture, ivec2(x, y), 0).rgb;
        w /= 1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722));            // tame HDR fireflies
        sum += c * w; wsum += w;
    }
    fragColor = vec4(sum / wsum, 1.0);
})";

#pragma endregion
}

MIDIRendererEnhanced::MIDIRendererEnhanced(MIDIApp* app) : AbstractMIDIRenderer(app)
{
    keyboardData.fill(RenderKeyboardKey3D());
    keyMetas.fill(KeyboardMeta3D());
}

MIDIRendererEnhanced::~MIDIRendererEnhanced()
{
    // the raw GL objects are not owned by any wrapper class (the unique_ptr members clean up after themselves).
    // this needs the GL context to be current, same as everything else in this class
    DestroySsrTargets();

    if (msaaFBO) glDeleteFramebuffers(1, &msaaFBO);
    if (msaaColorTexture) glDeleteTextures(1, &msaaColorTexture);
    if (msaaDepthRBO) glDeleteRenderbuffers(1, &msaaDepthRBO);

    if (keyGlowTex) glDeleteTextures(1, &keyGlowTex);
    if (giEmitTex) glDeleteTextures(1, &giEmitTex);
    if (fogLightTex) glDeleteTextures(1, &fogLightTex);
    if (fogNoiseTex) glDeleteTextures(1, &fogNoiseTex);
}

void MIDIRendererEnhanced::Initialize()
{
    AbstractMIDIRenderer::Initialize();

#pragma region Note buffers + data

    notesProgram = ShaderProgram::Create(notes3dvert, notes3dfrag);
    notesVAO = std::make_unique<VertexArray>();
    notesVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    notesIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    notesEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*notesVAO);

        // static quad verts
        notesVBO->Bind();
        notesVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        // index buffer
        notesEBO->Bind();
        notesEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        // instance buffer
        notesIBO->Bind();
        notesIBO->SetData(renderNotes, GL_DYNAMIC_DRAW);

        notesVAO->SetFloatAttribute(1, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, left));
        notesVAO->SetFloatAttribute(2, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, right));
        notesVAO->SetFloatAttribute(3, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, start));
        notesVAO->SetFloatAttribute(4, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, end));
        notesVAO->SetIntAttribute(5, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, color));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
    }

#pragma endregion

#pragma region Keyboard buffers + data
    // 1. Sort keys to render white keys first, then black keys
    std::vector<uint8_t> blackIDs;
    std::vector<uint8_t> whiteIDs;
    for (uint8_t key = 0; key < MIDI_KEYS; key++)
    {
        bool black = KEY_IS_BLACK(key);
        if (black) blackIDs.push_back(key);
        else whiteIDs.push_back(key);
        keyMetas[key].MarkBlack(black);
    }

    numWhiteKeys = whiteIDs.size();
    numBlackKeys = blackIDs.size();

    int i = 0;
    for (auto& white : whiteIDs) kbIDs[i++] = white;
    for (auto& black : blackIDs) kbIDs[i++] = black;

    keyboardProgram = ShaderProgram::Create(keyboard3dvert, keyboard3dfrag);

    // 1D light profile used for the fake GI + reflections (bound to texture unit 2)
    glowProfile.assign(KEY_GLOW_TEX_WIDTH * 3, 0.0f);
    glGenTextures(1, &keyGlowTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, keyGlowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, KEY_GLOW_TEX_WIDTH, 1, 0, GL_RGB, GL_FLOAT, glowProfile.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);

    // emitter list for the light scatter (bound to texture unit 3)
    giEmitData.assign(static_cast<size_t>(MIDI_KEYS) * 2 * 4, 0.0f);
    glGenTextures(1, &giEmitTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, giEmitTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, MIDI_KEYS, 2, 0, GL_RGBA, GL_FLOAT, giEmitData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);

    keyboardIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    keyboardIBO->Bind();
    keyboardIBO->SetData(keyboardData, GL_DYNAMIC_DRAW);

    whiteKeyVAO = std::make_unique<VertexArray>();
    whiteKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    whiteKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*whiteKeyVAO);
        auto* whiteMesh = Models::WhiteKeyMesh;

        whiteKeyVBO->Bind();
        whiteKeyVBO->SetData(whiteMesh->vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        whiteKeyEBO->Bind();
        whiteKeyEBO->SetData(whiteMesh->indices, GL_STATIC_DRAW);

        keyboardIBO->Bind();
        whiteKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, left));
        whiteKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, right));
        whiteKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, pressFactor));
        whiteKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, meta));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }

    blackKeyVAO = std::make_unique<VertexArray>();
    blackKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    blackKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*blackKeyVAO);
        auto* blackMesh = Models::BlackKeyMesh;

        blackKeyVBO->Bind();
        blackKeyVBO->SetData(blackMesh->vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        blackKeyEBO->Bind();
        blackKeyEBO->SetData(blackMesh->indices, GL_STATIC_DRAW);

        keyboardIBO->Bind();

        // shift the attribute pointers past the white keys
        size_t blackOffset = numWhiteKeys * sizeof(RenderKeyboardKey3D);

        blackKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, left));
        blackKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, right));
        blackKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, pressFactor));
        blackKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, meta));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }

#pragma endregion

#pragma region Saber buffers + data
    saberProgram = ShaderProgram::Create(saber3dvert, saber3dfrag);
    saberVAO = std::make_unique<VertexArray>();
    saberVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    saberEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*saberVAO);

        saberVBO->Bind();
        saberVBO->SetData(CUBE_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        saberEBO->Bind();
        saberEBO->SetData(CUBE_INDICES, GL_STATIC_DRAW);
    }
#pragma endregion

#pragma region Mist setup
    mistProgram = ShaderProgram::Create(mist3dvert, mist3dfrag);
    mistQuad = std::make_unique<Quad>();
    mistQuad->SetShader(mistProgram);

    CreateFogNoiseTexture();

    // pressed key light strip for the fog (texture unit 4), mip-mapped
    fogLightData.assign((size_t)FOG_LIGHT_TEX_WIDTH * FOG_LIGHT_ROWS * 3, 0.0f);
    glGenTextures(1, &fogLightTex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fogLightTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, FOG_LIGHT_TEX_WIDTH, FOG_LIGHT_ROWS, 0, GL_RGB, GL_FLOAT, fogLightData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);   // no mipmaps any more
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
#pragma endregion

#pragma region Particle buffers + data
    particleProgram = ShaderProgram::Create(particle3dvert, particle3dfrag);
    particleVAO = std::make_unique<VertexArray>();
    particleVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    particleIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    particleEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*particleVAO);

        particleVBO->Bind();
        particleVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        // index buffer
        particleEBO->Bind();
        particleEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        particleIBO->Bind();
        particleIBO->SetData(particlePool, GL_DYNAMIC_DRAW);

        particleVAO->SetFloatAttribute(1, 3, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, position));
        particleVAO->SetFloatAttribute(2, 4, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, color));
        particleVAO->SetFloatAttribute(3, 1, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, scale));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
    }
#pragma endregion

#pragma region Flare buffers + data
    flaresProgram = ShaderProgram::Create(flareVert, flareFrag);
    flaresVAO = std::make_unique<VertexArray>();
    flaresVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    flaresIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    flaresEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*flaresVAO);

        flaresVBO->Bind();
        flaresVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        flaresEBO->Bind();
        flaresEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        flaresIBO->Bind();
        flaresIBO->SetData(renderFlares, GL_DYNAMIC_DRAW);

        flaresVAO->SetFloatAttribute(1, 1, sizeof(RenderFlare), offsetof(RenderFlare, left));
        flaresVAO->SetFloatAttribute(2, 1, sizeof(RenderFlare), offsetof(RenderFlare, right));
        flaresVAO->SetIntAttribute(3, 1, sizeof(RenderFlare), offsetof(RenderFlare, color));
        flaresVAO->SetFloatAttribute(4, 1, sizeof(RenderFlare), offsetof(RenderFlare, alpha));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }
#pragma endregion

#pragma region post processing setup
    downsampleShader = ShaderProgram::Create(fullscreenvert, downsamplefrag);
    upsampleShader = ShaderProgram::Create(fullscreenvert, upsamplefrag);
    compositeShader = ShaderProgram::Create(fullscreenvert, compositefrag);

    screenQuad = std::make_unique<Quad>();
    hdrSceneFBO = std::make_unique<Framebuffer>();
    hdrSceneFBO->Setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    resolvedFBO = std::make_unique<Framebuffer>();
    resolvedFBO->Setup(outWidth, outHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    resolveShader = ShaderProgram::Create(fullscreenvert, resolvefrag);
#pragma endregion

#pragma region uniforms setup
    SetupUniforms();
#pragma endregion

    initialized = true;
    ApplyRenderSize();
}

void MIDIRendererEnhanced::SetupUniforms()
{
    {
        ShaderBind notesBind(*notesProgram);

        notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
        notesProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
        notesProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
        notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind keyboardBind(*keyboardProgram);

        keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
        keyboardProgram->SetFloat("keyboardBrightness", rendererSettings.keyboardBrightness);
        keyboardProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
        keyboardProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
        keyboardProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
        mistProgram->SetFloat("mistOpacity", rendererSettings.mistOpacity);
        mistProgram->SetFloat("mistSpeed", rendererSettings.mistSpeed);
        mistProgram->SetFloat("mistScale", rendererSettings.mistScale);
    }
}

void MIDIRendererEnhanced::LoadSequence(MIDISequence* sequence)
{
    std::lock_guard<std::mutex> lock(renderMutex);

    if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
    AbstractMIDIRenderer::LoadSequence(sequence);

    colors.LoadColors();

    for (auto& id : startBlockIDs)
        id = 0;

    lastTime = 0;
}

void MIDIRendererEnhanced::CalcKeyPosAndWidth()
{
    MIDIPlayerConfig* config = app->GetConfig();
    int keyFirst = config->render.GetKeyFirst();
    int keyLast = config->render.GetKeyLast();

    float rawPos[MIDI_KEYS];
    float rawWidth[MIDI_KEYS];
    float pos = 0.0f;
    for (int i = 0; i < MIDI_KEYS; i++)
    {
        rawPos[i] = pos;
        pos += keyPosDiff[i % 12];
    }

    int lastIdxWhiteUnit = -1;
    for (int j = 0; j < MIDI_KEYS; j++)
    {
        if (KEY_IS_BLACK(j))
        {
            rawWidth[j] = 75.0f / 115.0f; // black key width relative to a white key step
        }
        else
        {
            if (lastIdxWhiteUnit != -1)
                rawWidth[lastIdxWhiteUnit] = rawPos[j] - rawPos[lastIdxWhiteUnit];
            lastIdxWhiteUnit = j;
        }
    }
    if (lastIdxWhiteUnit != -1)
        rawWidth[lastIdxWhiteUnit] = 1.0f;

    float spanUnits = (rawPos[keyLast] + rawWidth[keyLast]) - rawPos[keyFirst];
    if (spanUnits < 0.001f)
        spanUnits = 0.001f;

    // full 0-127 span, used as the baseline that keyboardMaxZ was tuned against
    float fullSpanUnits = (rawPos[127] + rawWidth[127]) - rawPos[0];
    if (fullSpanUnits < 0.001f)
        fullSpanUnits = 0.001f;

    float noteWidth = (float)width / spanUnits; // pixels per white-key unit
    float noteWidthBlack = noteWidth * (75.0f / 115.0f);

    float firstKeyPos = rawPos[keyFirst] * noteWidth;
    for (int i = 0; i < MIDI_KEYS; i++)
        keyPos[i] = (rawPos[i] * noteWidth - firstKeyPos) / (float)width;

    int lastIdxWhite = -1;
    for (int j = 0; j < MIDI_KEYS; j++)
    {
        if (KEY_IS_BLACK(j))
            keyWidth[j] = noteWidthBlack / (float)width;
        else
        {
            if (lastIdxWhite != -1)
                keyWidth[lastIdxWhite] = keyPos[j] - keyPos[lastIdxWhite];
            lastIdxWhite = j;
        }
    }

    keyWidth[keyLast] = 1.0f - keyPos[keyLast];

    float aspect = (float)width / (float)height;
    float heightScale = fullSpanUnits / spanUnits;
    keyboardHeight = keyboardMaxZ * heightScale * 1.75f;

    whiteKeyStart = 0;
    numDrawWhiteKeys = 0;
    bool foundWhiteStart = false;
    for (size_t idx = 0; idx < numWhiteKeys; idx++)
    {
        uint8_t key = kbIDs[idx];
        if (key >= keyFirst && key <= keyLast)
        {
            if (!foundWhiteStart) { whiteKeyStart = idx; foundWhiteStart = true; }
            numDrawWhiteKeys++;
        }
    }

    blackKeyStart = 0;
    numDrawBlackKeys = 0;
    bool foundBlackStart = false;
    for (size_t idx = 0; idx < numBlackKeys; idx++)
    {
        uint8_t key = kbIDs[numWhiteKeys + idx];
        if (key >= keyFirst && key <= keyLast)
        {
            if (!foundBlackStart) { blackKeyStart = idx; foundBlackStart = true; }
            numDrawBlackKeys++;
        }
    }

    RebindKeyboardDrawRange();

    keyboardDirty = true;
}

void MIDIRendererEnhanced::RebindKeyboardDrawRange()
{
    keyboardIBO->Bind();

    {
        VertexArrayBind vaoBind(*whiteKeyVAO);
        size_t whiteBase = whiteKeyStart * sizeof(RenderKeyboardKey3D);
        whiteKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, left));
        whiteKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, right));
        whiteKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, pressFactor));
        whiteKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, meta));
    }

    {
        VertexArrayBind vaoBind(*blackKeyVAO);
        size_t blackOffset = numWhiteKeys * sizeof(RenderKeyboardKey3D);
        size_t blackBase = blackOffset + blackKeyStart * sizeof(RenderKeyboardKey3D);
        blackKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, left));
        blackKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, right));
        blackKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, pressFactor));
        blackKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, meta));
    }
}

void MIDIRendererEnhanced::UpdateKeyGlow(float /*dt*/)
{
    // must run before the press flags are cleared in UpdateKeyboardInstance
    constexpr int N = KEY_GLOW_TEX_WIDTH;
    std::fill(glowProfile.begin(), glowProfile.end(), 0.0f);

    // the reflection profile is instant too: a key is either lit or it is not
    const float sigma = 0.02f;
    const float invTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);
    const int reach = static_cast<int>(std::ceil(sigma * 3.0f * N));

    for (int id = 0; id < MIDI_KEYS; id++)
    {
        if (!keyMetas[id].pressed) continue;

        float center = keyPos[id] + keyWidth[id] * 0.5f;
        if (center < -0.1f || center > 1.1f) continue;

        const glm::vec3 kc = ShiftedKeyColor(UnpackKeyColor(keyMetas[id].color), rendererSettings, 0.5f);
        float r = kc.r;
        float g = kc.g;
        float b = kc.b;
        float weight = keyMetas[id].black ? 0.8f : 1.0f;

        int c0 = static_cast<int>(center * N);
        int xs = std::max(0, c0 - reach);
        int xe = std::min(N - 1, c0 + reach);
        for (int x = xs; x <= xe; x++)
        {
            float d = (x + 0.5f) / N - center;
            float w = std::exp(-d * d * invTwoSigmaSq) * weight;
            glowProfile[x * 3 + 0] += r * w;
            glowProfile[x * 3 + 1] += g * w;
            glowProfile[x * 3 + 2] += b * w;
        }
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, keyGlowTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, N, 1, GL_RGB, GL_FLOAT, glowProfile.data());
    glActiveTexture(GL_TEXTURE0);
}

void MIDIRendererEnhanced::UpdateGIEmitters()
{
    giEmitCount = 0;
    if (!rendererSettings.giEnabled) return;

    // key geometry, mirrors the keyboard vertex shader
    const float kbThickness = 0.20f;
    const float whiteTopY = 0.0175f * kbThickness;
    const float blackTopY = (0.035f + 0.001f) * kbThickness;

    constexpr size_t row1 = static_cast<size_t>(MIDI_KEYS) * 4;
    int count = 0;

    // keyboardData still holds this frame's pressed flag + colour (the flags in keyMetas are
    // already cleared by now), and it is exactly what the keys themselves are drawn with
    for (int i = 0; i < MIDI_KEYS; i++)
    {
        const RenderKeyboardKey3D& k = keyboardData[i];
        if (!(k.meta & KeyboardMeta3D::META_PRESSED)) continue;

        const float cx = (k.left + k.right) * 0.5f;
        if (cx < -0.1f || cx > 1.1f) continue;

        const bool black = (k.meta & KeyboardMeta3D::META_BLACK) != 0;
        const float w = k.right - k.left;
        const float depth = keyboardHeight * 2.2f * (black ? 0.15f : 0.25f) * 1.01f;
        const float topY = black ? blackTopY : whiteTopY;

        // emitter sits a little above the key top, as if it were the glowing volume above the key
        const float ex = cx;
        const float ey = topY + w * 0.35f;
        const float ez = keyboardZOffset - 0.01f + depth * 0.5f;

        // same radiance as the key's glowing outline
        const float s = rendererSettings.keyGlowFactor * 0.4f;
        // the keys are drawn with a hue shift that grows toward their front edge, so use the colour at mid key
        const glm::vec3 ec = ShiftedKeyColor(UnpackKeyColor(k.meta), rendererSettings, 0.5f) * s;
        const float r = ec.r;
        const float g = ec.g;
        const float b = ec.b;

        float* a0 = &giEmitData[static_cast<size_t>(count) * 4];
        a0[0] = r; a0[1] = g; a0[2] = b; a0[3] = 1.0f;
        float* a1 = &giEmitData[row1 + static_cast<size_t>(count) * 4];
        a1[0] = ex; a1[1] = ey; a1[2] = ez; a1[3] = w * depth;
        count++;
    }

    giEmitCount = count;
    if (count == 0) return;

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, giEmitTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, count, 1, GL_RGBA, GL_FLOAT, giEmitData.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, count, 1, GL_RGBA, GL_FLOAT, giEmitData.data() + row1);
    glActiveTexture(GL_TEXTURE0);
}

void MIDIRendererEnhanced::UpdateKeyboardInstance(double deltaTime)
{
    bool needsUpload = false;

    // cap the dt so physics does not explode lol
    float dt = static_cast<float>(std::min(deltaTime, 0.033));

    UpdateKeyGlow(dt);

    int i = 0;
    for (uint8_t id : kbIDs)
    {
        float targetPress = keyMetas[id].pressed ? 1.0f : 0.0f;
        float currentPress = keyboardData[i].pressFactor;
        float& velocity = keyMetas[id].velocity;

        bool pressing = targetPress > currentPress;
        float stiffness = pressing ? keyPressStiffness : keyReleaseStiffness;

        float force = stiffness * (targetPress - currentPress) - (keyDamping * velocity);
        velocity += force * dt;
        float newPress = currentPress + velocity * dt;

        if (newPress >= 1.0f)
        {
            newPress = 1.0f;
            velocity = 0.0f;
        }
        else if (newPress <= 0.0f && targetPress == 0.0f)
        {
            newPress = 0.0f;
            if (velocity < 0.0f)
            {
                velocity = -velocity * keyTopBounce;
            }
        }

        if (abs(newPress - targetPress) < 0.001f && std::abs(velocity) < 0.01f)
        {
            newPress = targetPress;
            velocity = 0.0f;
        }

        const uint32_t newMeta = keyMetas[id].GetMeta();
        if (currentPress != newPress || keyboardData[i].meta != newMeta)
        {
            needsUpload = true;
        }

        keyboardData[i].left = keyPos[id];
        keyboardData[i].right = keyPos[id] + keyWidth[id];
        keyboardData[i].pressFactor = newPress;
        keyboardData[i].meta = keyMetas[id].GetMeta();

        keyMetas[id].MarkPressed(false);
        i++;
    }

    if (needsUpload || keyboardDirty)
    {
        keyboardIBO->Bind();
        glBufferSubData(GL_ARRAY_BUFFER, 0, keyboardData.size() * sizeof(RenderKeyboardKey3D), keyboardData.data());
        keyboardDirty = false;
    }
}

void MIDIRendererEnhanced::UpdateKeyboardLayout()
{
    // camera distance is chosen so the visible width at the playing surface is exactly 1 (the keyboard width)
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float halfFov = glm::radians(rendererSettings.keyboardFOV) * 0.5f;

    cameraDistance = 1.0f / (2.0f * std::tan(halfFov) * aspect);

    const float visibleHeight = 2.0f * cameraDistance * std::tan(halfFov);
    const float keyDepthReach = keyboardHeight * 0.55f; // 2.2 * 0.25, matches the shader's actual multiplier
    keyboardZOffset = (visibleHeight / 2.0f) - keyDepthReach + keyboardBottomFill;

    // Where the notes end on screen: the middle of the saber, projected through the same camera the
    // keyboard is drawn with. The saber sits slightly above the playing surface, so under perspective it
    // lands a little further from the screen centre than the flat plane would, and that grows with the FOV.
    // (this replaces keyboardHeight * aspect / (16/9), which ignored the perspective)
    const float kbThickness = 0.20f;
    const float t = rendererSettings.saberThickness;
    const glm::vec3 saberCentre(0.5f,
        (0.035f + 0.001f) * kbThickness + t * 0.5f,
        keyboardZOffset - 0.01f - t * 0.5f);

    const glm::mat4 projection = glm::perspective(glm::radians(rendererSettings.keyboardFOV), aspect, 0.1f, 100.0f);
    const glm::vec4 clip = projection * GetViewMatrixFromEuler() * glm::vec4(saberCentre, 1.0f);
    keyboardTopNorm = clip.y / clip.w * 0.5f + 0.5f;
}

void MIDIRendererEnhanced::RenderKeyboard()
{
    ShaderBind kbBind(*keyboardProgram);

    // calculate required camera distance based on keyboard height and FOV
    // this ensures the keyboard spans the appropriate portion of the viewport
    float aspect = (float)width / (float)height;
    float verticalFOVRad = glm::radians(rendererSettings.keyboardFOV);

    UpdateKeyboardLayout();

    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);

    glm::mat4 view = GetViewMatrixFromEuler();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

    keyboardProgram->SetMat4("projection", projection);
    keyboardProgram->SetMat4("view", view);
    keyboardProgram->SetFloat("keyboardZOffset", keyboardZOffset);
    keyboardProgram->SetFloat("keyboardHeight", keyboardHeight);
    keyboardProgram->SetVec3("cameraPos", cameraPos);
    keyboardProgram->SetFloat("animTime", app->GetTimer()->Elapsed());
    keyboardProgram->SetFloat("reflectionStrength", rendererSettings.reflectionStrength);
    keyboardProgram->SetFloat("outlineScale", (float)width / outWidth);

    // light scatter: pressed keys (area emitters) + the saber (line emitter)
    UpdateGIEmitters();
    {
        const bool gi = rendererSettings.giEnabled;
        const float kbThickness = 0.20f;
        const float saberTopY = (0.035f + 0.001f) * kbThickness; // matches RenderSaber
        const float t = rendererSettings.saberThickness;

        // saber is a box of t x t cross-section whose back-bottom edge sits at (saberTopY, keyboardZOffset - 0.01)
        const float saberY = saberTopY + t * 0.5f;
        const float saberZ = keyboardZOffset - 0.01f - t * 0.5f;

        // radiance * emitting width per unit length
        glm::vec3 lambda = rendererSettings.saberColor * rendererSettings.saberBrightness * (t * 2.0f);

        keyboardProgram->SetInt("giEmitCount", gi ? giEmitCount : 0);
        keyboardProgram->SetFloat("giStrength", gi ? rendererSettings.giStrength : 0.0f);
        keyboardProgram->SetFloat("giRadius", rendererSettings.giRadius);
        keyboardProgram->SetFloat("giSaberStrength", gi ? rendererSettings.giSaberStrength : 0.0f);
        keyboardProgram->SetVec3("giSaberLambda", lambda);
        keyboardProgram->SetFloat("giSaberY", saberY);
        keyboardProgram->SetFloat("giSaberZ", saberZ);
        keyboardProgram->SetFloat("giSaberRadius", std::max(t, 0.004f));

        const float whiteDepth = keyboardHeight * 2.2f * 0.25f * 1.01f;   // mirrors the vertex shader
        keyboardProgram->SetFloat("keyZ0", keyboardZOffset - 0.01f);
        keyboardProgram->SetFloat("keyZ1", keyboardZOffset - 0.01f + whiteDepth);
        keyboardProgram->SetVec3("refSaberRadiance", rendererSettings.saberColor * rendererSettings.saberBrightness);
    }

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, giEmitTex);
    keyboardProgram->SetInt("giEmitTex", 3);
    glActiveTexture(GL_TEXTURE0);
    keyboardProgram->SetFloat("causticStrength", rendererSettings.causticStrength);
    keyboardProgram->SetFloat("causticSpeed", rendererSettings.causticSpeed);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, keyGlowTex);
    keyboardProgram->SetInt("keyGlowTex", 2);
    glActiveTexture(GL_TEXTURE0);

    auto drawKeys = [&]()
        {
            {
                VertexArrayBind whiteBind(*whiteKeyVAO);
                glDrawElementsInstanced(GL_TRIANGLES, Models::WhiteKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawWhiteKeys);
            }

            {
                VertexArrayBind blackBind(*blackKeyVAO);
                glDrawElementsInstanced(GL_TRIANGLES, Models::BlackKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawBlackKeys);
            }
        };

    // screen space reflections: keys only reflect keys, so the source buffer is a keyboard-only prepass
    const bool ssr = rendererSettings.ssrEnabled && kbSsrFBO != 0 && rendererSettings.ssrSteps > 0;

    keyboardProgram->SetMat4("viewProj", projection * view);
    keyboardProgram->SetFloat("ssrNear", 0.1f);   // must match glm::perspective above
    keyboardProgram->SetFloat("ssrFar", 100.0f);
    keyboardProgram->SetFloat("ssrMaxDist", rendererSettings.ssrMaxDistance);
    keyboardProgram->SetFloat("ssrThickness", rendererSettings.ssrThickness);
    keyboardProgram->SetFloat("ssrStrength", rendererSettings.ssrStrength);
    keyboardProgram->SetInt("ssrSteps", ssr ? std::clamp(rendererSettings.ssrSteps, 4, 64) : 0);
    keyboardProgram->SetInt("ssrColorTex", 6);
    keyboardProgram->SetInt("ssrDepthTex", 7);
    keyboardProgram->SetInt("ssrPass", 0);

    if (ssr)
    {
        // the targets are attached to the FBO we are about to draw into, so make sure they are not bound for sampling
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);

        glBindFramebuffer(GL_FRAMEBUFFER, kbSsrFBO);
        glViewport(0, 0, width, height);

        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const float clearDepth = 1.0f;
        glClearBufferfv(GL_COLOR, 0, clearColor);
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);

        keyboardProgram->SetInt("ssrPass", 1);
        drawKeys();
        keyboardProgram->SetInt("ssrPass", 0);

        hdrSceneFBO->Bind();
        glViewport(0, 0, width, height);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, kbSsrColorTex);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, kbSsrDepthTex);
        glActiveTexture(GL_TEXTURE0);
    }

    drawKeys();
}

void MIDIRendererEnhanced::CreateSsrTargets()
{
    DestroySsrTargets();

    glGenTextures(1, &kbSsrColorTex);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, kbSsrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &kbSsrDepthTex);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, kbSsrDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);

    glGenFramebuffers(1, &kbSsrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, kbSsrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, kbSsrColorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, kbSsrDepthTex, 0);

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // an incomplete target just turns SSR off (RenderKeyboard checks kbSsrFBO)
    if (!complete) DestroySsrTargets();
}

void MIDIRendererEnhanced::DestroySsrTargets()
{
    if (kbSsrFBO) glDeleteFramebuffers(1, &kbSsrFBO);
    if (kbSsrColorTex) glDeleteTextures(1, &kbSsrColorTex);
    if (kbSsrDepthTex) glDeleteTextures(1, &kbSsrDepthTex);
    kbSsrFBO = 0;
    kbSsrColorTex = 0;
    kbSsrDepthTex = 0;
}

void MIDIRendererEnhanced::RenderNotes()
{
    NoteFrame f;
    if (!BeginNoteFrame(f)) return;

    ShaderBind shaderBind(*notesProgram);
    VertexArrayBind notesVAOBind(*notesVAO);
    BufferBind notesIBOBind(*notesIBO);
    BufferBind notesVBOBind(*notesVBO);
    BufferBind notesEBOBind(*notesEBO);

    notesProgram->SetFloat("kbHeight", keyboardTopNorm);
    notesProgram->SetFloat("animTime", static_cast<float>(f.playbackSeconds));
    notesProgram->SetFloat("causticStrength", rendererSettings.causticStrength);
    notesProgram->SetFloat("causticScale", rendererSettings.causticScale);
    notesProgram->SetFloat("causticSpeed", rendererSettings.causticSpeed);
    notesProgram->SetFloat("outlineScale", (float)width / outWidth);

    const double kbNorm = (double)keyboardTopNorm;
    const double noteAreaPx = (double)height * (1.0 - kbNorm);
    const int samples = GetMSAASamples();
    const double rowScale = samples >= 16 ? 4.0 : samples >= 8 ? 3.0 : samples >= 4 ? 2.0 : 1.0;

    CollectNotes(f, { .noteAreaPx = noteAreaPx, .yBase = noteAreaPx, .rowScale = rowScale },
        [&](uint8_t id, uint16_t track, uint8_t channel)
        {
            keyMetas[id].MarkPressed(true);
            keyMetas[id].color = colors.GetColor(track, channel);
        });

    size_t noteID = 0, batchCount = 0;
    ForEachNote(f, [&](const NoteRef& n)
        {
            renderNotes[noteID++] = RenderNote3D(
                keyPos[n.id],
                keyPos[n.id] + keyWidth[n.id],
                (float)n.yStart, (float)n.yEnd,
                colors.GetColor(n.track, n.channel)
            );

            if (noteID >= NOTE_BUFFER_SIZE)
            {
                UploadNoteBuffer(NOTE_BUFFER_SIZE);
                if (++batchCount >= NOTES_MAX_BATCHES) { glFlush(); batchCount = 0; }
                noteID = 0;
            }
        });
    if (noteID != 0) UploadNoteBuffer(noteID);

    EndNoteFrame(f);
    lastTime = f.tick;
}

void MIDIRendererEnhanced::RenderSaber()
{
    ShaderBind saberBind(*saberProgram);

    float aspect = (float)width / (float)height;
    float verticalFOVRad = glm::radians(rendererSettings.keyboardFOV);
    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);
    glm::mat4 view = GetViewMatrixFromEuler();

    float kbThickness = 0.20f;
    float blackKeyHeight = 0.035f * kbThickness;
    float blackKeyElevation = 0.001f * kbThickness;
    float exactTopY = blackKeyHeight + blackKeyElevation;

    float saberZ = keyboardZOffset - 0.01f;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, exactTopY, saberZ));
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -rendererSettings.saberThickness));
    model = glm::scale(model, glm::vec3(1.0f, rendererSettings.saberThickness, rendererSettings.saberThickness));

    saberProgram->SetMat4("projection", projection);
    saberProgram->SetMat4("view", view);
    saberProgram->SetMat4("model", model);

    // apply bloom overdrive color
    glm::vec3 glowingColor = rendererSettings.saberColor * rendererSettings.saberBrightness;
    saberProgram->SetVec3("saberColor", glowingColor);

    VertexArrayBind vaoBind(*saberVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void MIDIRendererEnhanced::CreateFogNoiseTexture()
{
    // tileable fractal value noise, 4 independent channels (density + 3 warp channels)
    constexpr int S = FOG_NOISE_SIZE;
    constexpr int OCTAVES = 3;
    constexpr int cells[OCTAVES] = { 4, 8, 16 };
    constexpr float weight[OCTAVES] = { 0.57f, 0.29f, 0.14f };

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> lattice[4][OCTAVES];
    for (int ch = 0; ch < 4; ch++)
    {
        for (int o = 0; o < OCTAVES; o++)
        {
            lattice[ch][o].resize(static_cast<size_t>(cells[o]) * cells[o] * cells[o]);
            for (float& v : lattice[ch][o]) v = dist(rng);
        }
    }

    auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };

    struct Axis { int i0, i1; float t; };
    auto axis = [&](int texel, int n) -> Axis
        {
            const float u = static_cast<float>(texel) / S * n;
            const int i = static_cast<int>(std::floor(u));
            return { i % n, (i + 1) % n, fade(u - i) };
        };

    std::vector<uint8_t> pixels(static_cast<size_t>(S) * S * S * 4);

    for (int z = 0; z < S; z++)
    {
        Axis az[OCTAVES];
        for (int o = 0; o < OCTAVES; o++) az[o] = axis(z, cells[o]);

        for (int y = 0; y < S; y++)
        {
            Axis ay[OCTAVES];
            for (int o = 0; o < OCTAVES; o++) ay[o] = axis(y, cells[o]);

            for (int x = 0; x < S; x++)
            {
                float result[4] = { 0, 0, 0, 0 };

                for (int o = 0; o < OCTAVES; o++)
                {
                    const int n = cells[o];
                    const Axis ax = axis(x, n);
                    const Axis& ayo = ay[o];
                    const Axis& azo = az[o];

                    for (int ch = 0; ch < 4; ch++)
                    {
                        const std::vector<float>& l = lattice[ch][o];
                        auto at = [&](int xi, int yi, int zi) { return l[(static_cast<size_t>(zi) * n + yi) * n + xi]; };

                        const float c00 = at(ax.i0, ayo.i0, azo.i0) + (at(ax.i1, ayo.i0, azo.i0) - at(ax.i0, ayo.i0, azo.i0)) * ax.t;
                        const float c10 = at(ax.i0, ayo.i1, azo.i0) + (at(ax.i1, ayo.i1, azo.i0) - at(ax.i0, ayo.i1, azo.i0)) * ax.t;
                        const float c01 = at(ax.i0, ayo.i0, azo.i1) + (at(ax.i1, ayo.i0, azo.i1) - at(ax.i0, ayo.i0, azo.i1)) * ax.t;
                        const float c11 = at(ax.i0, ayo.i1, azo.i1) + (at(ax.i1, ayo.i1, azo.i1) - at(ax.i0, ayo.i1, azo.i1)) * ax.t;
                        const float c0 = c00 + (c10 - c00) * ayo.t;
                        const float c1 = c01 + (c11 - c01) * ayo.t;
                        result[ch] += (c0 + (c1 - c0) * azo.t) * weight[o];
                    }
                }

                uint8_t* px = &pixels[((static_cast<size_t>(z) * S + y) * S + x) * 4];
                for (int ch = 0; ch < 4; ch++)
                    px[ch] = static_cast<uint8_t>(std::clamp(result[ch], 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }

    glGenTextures(1, &fogNoiseTex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_3D, fogNoiseTex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, S, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glActiveTexture(GL_TEXTURE0);
}

void MIDIRendererEnhanced::UpdateFogLight()
{
    constexpr int N = FOG_LIGHT_TEX_WIDTH, R = FOG_LIGHT_ROWS;
    std::fill(fogLightData.begin(), fogLightData.end(), 0.0f);

    const float s = rendererSettings.keyGlowFactor * 0.4f;
    const float logRatio = std::log(FOG_SIGMA_MAX / FOG_SIGMA_MIN);

    for (int id = 0; id < MIDI_KEYS; id++)
    {
        if (!keyMetas[id].pressed) continue;
        const float left = keyPos[id], right = keyPos[id] + keyWidth[id];
        if (right < 0.0f || left > 1.0f) continue;

        const glm::vec3 col = ShiftedKeyColor(UnpackKeyColor(keyMetas[id].color), rendererSettings, 0.0f)
            * (keyMetas[id].black ? 0.8f : 1.0f) * s;

        for (int r = 0; r < R; r++)
        {
            const float sigma = FOG_SIGMA_MIN * std::exp(logRatio * r / (R - 1));
            const float inv = 1.0f / (sigma * 1.41421356f);
            const int xs = std::max(0, (int)std::floor((left - 3.0f * sigma) * N));
            const int xe = std::min(N - 1, (int)std::ceil((right + 3.0f * sigma) * N));
            float* row = &fogLightData[(size_t)r * N * 3];
            for (int x = xs; x <= xe; x++)
            {
                const float px = (x + 0.5f) / N;
                // box [left, right] convolved with a gaussian
                const float w = 0.5f * (std::erf((px - left) * inv) - std::erf((px - right) * inv));
                row[x * 3 + 0] += col.r * w;
                row[x * 3 + 1] += col.g * w;
                row[x * 3 + 2] += col.b * w;
            }
        }
    }

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fogLightTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, N, R, GL_RGB, GL_FLOAT, fogLightData.data());
    glActiveTexture(GL_TEXTURE0);
}

void MIDIRendererEnhanced::RenderMist()
{
    UpdateFogLight();

    // same note area the notes are drawn in (see RenderNotes)
    const float kbNorm = keyboardTopNorm;
    const float fogHeight = std::clamp(rendererSettings.mistHeight, 0.05f, 1.0f);
    const float quadHeight = (1.0f - kbNorm) * fogHeight;

    // the fog only covers the part of the screen it can actually reach, which is what keeps it fast.
    // in world units (keyboard width = 1) that area is quadHeight * height / width tall.
    const float fogYScale = quadHeight * static_cast<float>(height) / static_cast<float>(width);

    // light geometry, mirrors RenderKeyboard / UpdateGIEmitters, relative to the playing surface
    const float kbThickness = 0.20f;
    const float t = rendererSettings.saberThickness;
    const float saberY = (0.035f + 0.001f) * kbThickness + t * 0.5f;
    const float keyY = 0.0175f * kbThickness + 0.007f;
    const float keyZ = keyboardHeight * 2.2f * 0.25f * 1.01f * 0.5f + t * 0.5f; // middle of a white key, measured from the saber
    const glm::vec3 saberLambda = rendererSettings.saberColor * rendererSettings.saberBrightness * (t * 2.0f);

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
        mistProgram->SetFloat("mistOpacity", rendererSettings.mistOpacity);
        mistProgram->SetFloat("mistSpeed", rendererSettings.mistSpeed);
        mistProgram->SetFloat("mistScale", rendererSettings.mistScale);
        mistProgram->SetFloat("animTime", app->GetTimer()->Elapsed());

        mistProgram->SetFloat("fogBottom", kbNorm);
        mistProgram->SetFloat("fogQuadH", quadHeight);
        mistProgram->SetFloat("fogYScale", fogYScale);
        mistProgram->SetFloat("fogDepth", rendererSettings.mistDepth);
        mistProgram->SetInt("fogSteps", std::clamp(rendererSettings.mistSteps, 4, 48));
        mistProgram->SetFloat("fogLightStrength", rendererSettings.mistLightStrength);
        mistProgram->SetFloat("fogLightReach", rendererSettings.mistLightReach);
        mistProgram->SetFloat("fogAmbient", rendererSettings.mistAmbient);
        mistProgram->SetVec3("fogSaberLambda", saberLambda);
        mistProgram->SetFloat("fogSaberRadius", std::max(t, 0.004f));
        mistProgram->SetFloat("fogSaberY", saberY);
        mistProgram->SetFloat("fogKeyY", keyY);
        mistProgram->SetFloat("fogKeyZ", keyZ);
        mistProgram->SetFloat("fogSigmaMin", FOG_SIGMA_MIN);
        mistProgram->SetFloat("fogSigmaLogRatio", std::log(FOG_SIGMA_MAX / FOG_SIGMA_MIN));
        mistProgram->SetFloat("fogLightRows", (float)FOG_LIGHT_ROWS);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, fogLightTex);
        mistProgram->SetInt("fogLight", 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_3D, fogNoiseTex);
        mistProgram->SetInt("fogNoise", 5);

        glActiveTexture(GL_TEXTURE0);
    }

    // the quad's rectangle comes from fogBottom / fogQuadH in the vertex shader, so keep the transform neutral
    mistQuad->SetTransform({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f) });
    mistQuad->Draw();
}

void MIDIRendererEnhanced::EmitNoteExplosion(uint8_t keyID, uint32_t hexColor)
{
    float r = ((hexColor >> 16) & 0xFF) / 255.0f;
    float g = ((hexColor >> 8) & 0xFF) / 255.0f;
    float b = (hexColor & 0xFF) / 255.0f;

    const auto& pSettings = rendererSettings.particleSettings;

    glm::vec3 hdrColor = glm::vec3(r, g, b) * pSettings.brightness;

    float spawnX = keyPos[keyID];
    int count = pSettings.emission;

    const float spreadDegrees = 30.0f;
    const float minAngle = 90.0f - spreadDegrees * 0.5f;
    const float maxAngle = 90.0f + spreadDegrees * 0.5f;

    for (int i = 0; i < count; i++)
    {
        if (liveParticleCount >= PARTICLE_BUFFER_SIZE) break;
        Particle3D& p = particlePool[liveParticleCount++];
        p.position = glm::vec3(spawnX + ((rand() / (float)RAND_MAX)) * keyWidth[keyID], keyboardTopNorm, 0.0f);

        float angle = glm::radians(minAngle + ((rand() / (float)RAND_MAX) * (maxAngle - minAngle)));
        float speed = RandRange(0.05f, 0.15f) * pSettings.initialSpeed;

        p.velocity.x = cos(angle) * speed * 0.2f;
        p.velocity.y = sin(angle) * speed * 1.2f;

        p.curveSeed = RandRange(0.0f, 6.2831853f);

        // turbulenceVariance controls how much curveSpeed/curveAmp differ from one
        // particle to the next: 0 makes every particle wobble identically, 1 gives
        // the widest spread. turbulence is the overall master strength on top of that.
        const float speedCenter = 5.5f, speedHalfRange = 1.5f;
        const float ampCenter = 0.055f, ampHalfRange = 0.025f;
        float variance = std::clamp(pSettings.turbulenceVariance, 0.0f, 1.0f);

        p.curveSpeed = speedCenter + RandRange(-speedHalfRange, speedHalfRange) * variance;
        p.curveAmp = (ampCenter + RandRange(-ampHalfRange, ampHalfRange) * variance) * pSettings.turbulence;

        p.color = glm::vec4(hdrColor, 1.0f);

        // lifetime
        float lifetimeVar = pSettings.lifetimeVariance;

        float minLife = lifetimeVar * pSettings.lifetime;
        float maxLife = pSettings.lifetime;

        p.maxLife = RandRange(minLife, maxLife);

        p.life = p.maxLife;
        p.scale = 0.002f;
    }
}

void MIDIRendererEnhanced::UpdateParticles(double deltaTime)
{
    const auto& pSettings = rendererSettings.particleSettings;
    float swirlTime = static_cast<float>(app->GetTimer()->Elapsed()) * pSettings.swirlSpeed;

    const auto& ProcessParticles = [this, deltaTime, pSettings, swirlTime](size_t start, size_t end) {
        float dt = static_cast<float>(deltaTime);

        for (size_t i = start; i < end; i++)
        {
            Particle3D& p = particlePool[i];
            p.life -= dt;
            if (p.life <= 0.0f)
            {
                continue;
            }

            float t = p.maxLife - p.life;
            float curve = std::sin(t * p.curveSpeed + p.curveSeed) * p.curveAmp;
            p.velocity.x += curve * dt;

            // gravity pulls particles down (+gravity sinks them), wind pushes sideways
            p.velocity.y -= pSettings.gravityFactor * dt;

            // swirl: samples a time-varying curl noise field at the particle's current
            // position, so particles drift through shared, evolving vortices instead of
            // orbiting a fixed anchor.
            if (pSettings.swirlStrength != 0.0f)
            {
                glm::vec2 curl = SampleCurlNoise(p.position.x * pSettings.swirlScale, p.position.y * pSettings.swirlScale, swirlTime);
                p.velocity.x += curl.x * pSettings.swirlStrength * dt;
                p.velocity.y += curl.y * pSettings.swirlStrength * dt;
            }

            // drag exponentially bleeds off velocity over time
            float dragFactor = std::max(0.0f, 1.0f - pSettings.drag * dt);
            p.velocity *= dragFactor;

            p.position += p.velocity * dt;
            p.color.a = p.life / p.maxLife;
            p.scale = p.life / p.maxLife * 0.002f;
        }
        };

    if (liveParticleCount > 0)
    {
        constexpr size_t CHUNK_SIZE = 4096;

        for (size_t i = 0; i < liveParticleCount; i += CHUNK_SIZE)
        {
            size_t end = std::min(i + CHUNK_SIZE, liveParticleCount);
            particleThreadPool.SubmitJob([&, i, end] {
                ProcessParticles(i, end);
                });
        }

        particleThreadPool.WaitForAllJobs();

        size_t i = 0;
        while (i < liveParticleCount)
        {
            if (particlePool[i].life <= 0.0f)
            {
                particlePool[i] = particlePool[--liveParticleCount];
            }
            else
            {
                ++i;
            }
        }
    }

    MIDIPlayerConfig* config = app->GetConfig();
    int keyFirst = config->render.GetKeyFirst();
    int keyLast = config->render.GetKeyLast();

    for (uint8_t id : kbIDs)
    {
        if (id < keyFirst || id > keyLast) continue;

        const auto& key = keyMetas[id];
        float& timer = particleEmissionTimers[id];
        if (!key.pressed)
        {
            timer = 0.0f;
            continue;
        }
        uint32_t keyColor = key.color;

        if (timer == 0.0f)
        {
            EmitNoteExplosion(id, keyColor);
        }

        timer += deltaTime;
        while (timer >= pSettings.spawnRate)
        {
            EmitNoteExplosion(id, keyColor);
            timer -= pSettings.spawnRate;
        }
    }
}

void MIDIRendererEnhanced::RenderParticles()
{
    if (liveParticleCount == 0) return;

    for (size_t i = 0; i < liveParticleCount; i++)
    {
        particleGpuData[i] = { particlePool[i].position, particlePool[i].color, particlePool[i].scale };
    }

    particleIBO->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, 0, liveParticleCount * sizeof(RenderParticleInstance3D), particleGpuData.data());

    ShaderBind pBind(*particleProgram);
    VertexArrayBind pVAOBind(*particleVAO);
    BufferBind pVBOBind(*particleVBO);
    BufferBind pEBOBind(*particleEBO);

    particleProgram->SetFloat("aspect", (float)width / (float)height);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, liveParticleCount);
}

void MIDIRendererEnhanced::RenderFlares(double deltaTime)
{
    // for now just go through the keyboard and borrow states from there
    // TODO: move uniform calls to rendersettings
    float dt = static_cast<float>(std::min(deltaTime, 0.033));
    float fadeRate = dt / std::max(rendererSettings.flareFadeDuration, 0.0001f);

    size_t flaresToRender = 0;
    for (uint8_t id : kbIDs)
    {
        const auto& key = keyMetas[id];
        float target = key.pressed ? 1.0f : 0.0f;
        float& fade = flareFade[id];

        // linear ramp toward 0 or 1 over flareFadeDuration seconds, so a press fades
        // the flare in and a release fades it back out instead of popping instantly
        if (target > fade)
            fade = std::min(target, fade + fadeRate);
        else
            fade = std::max(target, fade - fadeRate);

        if (fade <= 0.0f) continue;

        float left = keyPos[id];
        float right = left + keyWidth[id];

        renderFlares[flaresToRender++] = RenderFlare(
            left,
            right,
            key.color,
            fade
        );
    }

    if (flaresToRender == 0) return;

    flaresIBO->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, 0, flaresToRender * sizeof(RenderFlare), renderFlares.data());

    ShaderBind flaresBind(*flaresProgram);
    VertexArrayBind vaoBind(*flaresVAO);
    BufferBind vboBind(*flaresVBO);
    BufferBind eboBind(*flaresEBO);

    flaresProgram->SetFloat("kbHeight", keyboardTopNorm);
    flaresProgram->SetFloat("flareHeight", rendererSettings.flareHeight);
    flaresProgram->SetFloat("flareBrightness", rendererSettings.flareBrightness);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, flaresToRender);
}

void MIDIRendererEnhanced::Render(double deltaTime)
{
    if (!initialized) return;

    MIDIPlayerConfig* config = app->GetConfig();
    if (config->render.ConsumeKeyRangeChanged())
    {
        liveParticleCount = 0;
        CalcKeyPosAndWidth();
    }

    UpdateKeyboardLayout();

    hdrSceneFBO->Bind();
    glViewport(0, 0, width, height);
    glDisable(GL_MULTISAMPLE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    RenderNotes();

    if (rendererSettings.mistEnabled)
    {
        // premultiplied over: the fog adds its scattered light and hides a bit of the notes behind it.
        // (for a purely additive fog use glBlendFunc(GL_ONE, GL_ONE))
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        RenderMist();
        glDisable(GL_BLEND);
    }

    if (rendererSettings.flaresEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        RenderFlares(deltaTime);
        glDisable(GL_BLEND);
    }

    if (rendererSettings.particlesEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        UpdateParticles(deltaTime);
        RenderParticles();
        glDisable(GL_BLEND);
    }

    glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    UpdateKeyboardInstance(deltaTime);
    RenderKeyboard();
    RenderSaber();

    glDisable(GL_DEPTH_TEST);

    hdrSceneFBO->Unbind();

    const bool ss = (width != outWidth || height != outHeight);
    GLuint sceneTex = hdrSceneFBO->GetSceneTexture();

    if (ss)
    {
        resolvedFBO->Bind();
        glViewport(0, 0, outWidth, outHeight);
        ShaderBind b(*resolveShader);
        resolveShader->SetInt("srcTexture", 0);
        resolveShader->SetVec2("scale", glm::vec2((float)width / outWidth, (float)height / outHeight));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);
        screenQuad->SetShader(resolveShader); screenQuad->Draw();

        sceneTex = resolvedFBO->GetSceneTexture();
    }

    glDisable(GL_DEPTH_TEST);

    {
        ShaderBind downsampleBind(*downsampleShader);
        downsampleShader->SetInt("srcTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);

        screenQuad->SetShader(downsampleShader);
        for (int i = 0; i < bloomChain.size(); i++) {
            bloomChain[i]->Bind();
            glViewport(0, 0, bloomChain[i]->GetWidth(), bloomChain[i]->GetHeight());
            downsampleShader->SetInt("mipLevel", i);
            screenQuad->Draw();

            glBindTexture(GL_TEXTURE_2D, bloomChain[i]->GetSceneTexture()); // Set texture for NEXT pass
        }
    }

    {
        ShaderBind upsampleBind(*upsampleShader);

        upsampleShader->SetInt("srcTexture", 0);
        upsampleShader->SetFloat("filterRadius", 1.5f); // could be adjustable but eh

        // Enable additive blending so the bloom layers stack
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        screenQuad->SetShader(upsampleShader);
        for (int i = bloomChain.size() - 1; i > 0; i--) {
            bloomChain[i - 1]->Bind(); // Bind the larger FBO
            glViewport(0, 0, bloomChain[i - 1]->GetWidth(), bloomChain[i - 1]->GetHeight());

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bloomChain[i]->GetSceneTexture()); // Read from smaller FBO

            screenQuad->Draw();
        }
        glDisable(GL_BLEND);
    }


    sceneFramebuffer->Bind();
    glViewport(0, 0, outWidth, outHeight);

    {
        ShaderBind compositeBind(*compositeShader);
        compositeShader->SetInt("sceneTex", 0);
        compositeShader->SetInt("bloomTex", 1);
        compositeShader->SetFloat("exposure", rendererSettings.exposure);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomChain[0]->GetSceneTexture());

        screenQuad->SetShader(compositeShader);
        screenQuad->Draw();
    }

    sceneFramebuffer->Unbind();

}

void MIDIRendererEnhanced::RenderSettings()
{
    if (ImGui::BeginTabBar("##renderSettings"))
    {
        if (ImGui::BeginTabItem("Visual"))
        {
            {
                ForumLayout layout("##rendering");
                layout.AddHeader("Rendering");

                SliderFloatField(layout, "##exposure", "Exposure", nullptr,
                    rendererSettings.exposure, 0.05f, 5.0f);

                layout.AddField("##ssaa", "Anti-aliasing",
                    [&]()
                    {
                        float prevSupersample = rendererSettings.supersample;
                        for (const auto& [label, val] : supersampleValues)
                        {
                            if (ImGui::RadioButton(label, rendererSettings.supersample == val))
                                rendererSettings.supersample = val;
                        }
                        if (prevSupersample != rendererSettings.supersample)
                            ApplyRenderSize();
                    });
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Notes"))
        {
            {
                ForumLayout layout("##notes");

                layout.AddHeader("Note Appearance");

                if (SliderFloatField(layout, "##noteOutlineGlow", "Outline emission", nullptr,
                    rendererSettings.noteOutlineGlowFactor, 1.0f, 8.0f))
                {
                    ShaderBind notesBind(*notesProgram);
                    notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
                }

                layout.AddHeader("Caustics");

                SliderFloatField(layout, "##causticStrength", "Caustic strength",
                    "Brightness of the caustic light pattern on notes and pressed keys. 0 disables it.",
                    rendererSettings.causticStrength, 0.0f, 3.0f);

                SliderFloatField(layout, "##causticScale", "Caustic scale",
                    "Higher values make a finer, tighter caustic pattern.",
                    rendererSettings.causticScale, 1.0f, 20.0f);

                SliderFloatField(layout, "##causticSpeed", "Caustic speed",
                    "How fast the caustic pattern shimmers and drifts.",
                    rendererSettings.causticSpeed, 0.0f, 3.0f);

                layout.AddHeader("HSV Shift");

                layout.AddFieldWithTooltip("##hsvShift", "Enable HSV Shift",
                    "Applies a hue, saturation, and value gradient shift to notes.",
                    [&]()
                    {
                        if (ImGui::Checkbox("", &rendererSettings.hsvShiftEnabled))
                        {
                            {
                                ShaderBind notesBind(*notesProgram);
                                notesProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
                            }

                            {
                                ShaderBind keyboardBind(*keyboardProgram);
                                keyboardProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
                            }
                        }
                    });

                if (rendererSettings.hsvShiftEnabled)
                {
                    if (SliderFloatField(layout, "##hsvStrength", "Strength", nullptr,
                        rendererSettings.hsvShiftStrength, 0.0f, 1.0f))
                    {
                        {
                            ShaderBind notesBind(*notesProgram);
                            notesProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
                        }

                        {
                            ShaderBind keyboardBind(*keyboardProgram);
                            keyboardProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
                        }
                    }

                    bool shiftsChanged = false;
                    shiftsChanged |= SliderFloatField(layout, "##hsvHueShift", "Hue shift", nullptr,
                        rendererSettings.hsvShifts.x, -1.0f, 1.0f);
                    shiftsChanged |= SliderFloatField(layout, "##hsvSatShift", "Saturation shift", nullptr,
                        rendererSettings.hsvShifts.y, -1.0f, 1.0f);
                    shiftsChanged |= SliderFloatField(layout, "##hsvValShift", "Value shift", nullptr,
                        rendererSettings.hsvShifts.z, -1.0f, 1.0f);

                    if (shiftsChanged)
                    {
                        {
                            ShaderBind notesBind(*notesProgram);
                            notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
                        }

                        {
                            ShaderBind keyboardBind(*keyboardProgram);
                            keyboardProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Keyboard"))
        {
            {
                ForumLayout layout("##keyboardAppearance");
                layout.AddHeader("Keyboard");

                if (SliderFloatField(layout, "##keyGlowFactor", "Key glow", nullptr,
                    rendererSettings.keyGlowFactor, 1.0f, 8.0f))
                {
                    ShaderBind keyboardBind(*keyboardProgram);
                    keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
                }

                if (SliderFloatField(layout, "##keyboardBrightness", "Brightness", nullptr,
                    rendererSettings.keyboardBrightness, 0.1f, 1.0f))
                {
                    ShaderBind keyboardBind(*keyboardProgram);
                    keyboardProgram->SetFloat("keyboardBrightness", rendererSettings.keyboardBrightness);
                }

                SliderFloatField(layout, "##keyboardFOV", "Field of View", nullptr,
                    rendererSettings.keyboardFOV, 20.0f, 89.9f);

                SliderFloatField(layout, "##reflectionStrength", "Reflections",
                    "Strength of the glossy lacquer reflections on the keys.",
                    rendererSettings.reflectionStrength, 0.0f, 3.0f);

                layout.AddHeader("Screen-Space Reflections");

                layout.AddField("##ssrEnabled", "Enable SSR",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.ssrEnabled);
                    });

                if (rendererSettings.ssrEnabled)
                {
                    SliderFloatField(layout, "##ssrStrength", "SSR gain",
                        "Brightness of what keys reflect from their neighbours, 1 = exactly as the neighbour looks. Raise it to make reflected glow stand out. Only keys are reflected, never notes.",
                        rendererSettings.ssrStrength, 0.0f, 4.0f);

                    SliderFloatField(layout, "##ssrMaxDistance", "SSR distance",
                        "How far a reflection ray travels. Longer reaches further keys but costs precision.",
                        rendererSettings.ssrMaxDistance, 0.02f, 0.4f);

                    SliderIntField(layout, "##ssrSteps", "SSR quality",
                        "Ray march steps per pixel. Lower values are faster but can miss thin edges.",
                        rendererSettings.ssrSteps, 8, 64);

                    SliderFloatField(layout, "##ssrThickness", "SSR thickness",
                        "How far behind a key surface a ray can pass and still count as hitting it.",
                        rendererSettings.ssrThickness, 0.002f, 0.05f);
                }

                layout.AddHeader("Light Scatter");

                layout.AddField("##giEnabled", "Enable light scatter",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.giEnabled);
                    });

                if (rendererSettings.giEnabled)
                {
                    SliderFloatField(layout, "##giStrength", "Key scatter",
                        "How strongly pressed keys light up the keys around them. Appears instantly with the note.",
                        rendererSettings.giStrength, 0.0f, 3.0f);

                    SliderFloatField(layout, "##giSaberStrength", "Saber scatter",
                        "How strongly the saber lights up the keyboard.",
                        rendererSettings.giSaberStrength, 0.0f, 5.0f);

                    SliderFloatField(layout, "##giRadius", "Light size",
                        "Size of each key's light. Larger spreads the same light wider and softer.",
                        rendererSettings.giRadius, 0.002f, 0.1f);
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Saber"))
        {
            {
                ForumLayout layout("##saberAppearance");
                layout.AddHeader("Saber Appearance");

                layout.AddField("##saberColor", "Color",
                    [&]()
                    {
                        if (ImGui::ColorEdit3("", &rendererSettings.saberColor.x))
                        {
                            ShaderBind mistBind(*mistProgram);
                            mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
                        }
                    });

                SliderFloatField(layout, "##saberBrightness", "Brightness", nullptr,
                    rendererSettings.saberBrightness, 0.0f, 20.0f);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mist"))
        {
            {
                ForumLayout layout("##mist");
                layout.AddHeader("Volumetric Fog");

                layout.AddField("##enableMist", "Enable fog",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.mistEnabled);
                    });

                if (rendererSettings.mistEnabled)
                {
                    layout.AddHeader("Appearance");

                    SliderFloatField(layout, "##mistOpacity", "Density",
                        "How thick the fog is.",
                        rendererSettings.mistOpacity, 0.0f, 2.0f);

                    SliderFloatField(layout, "##mistScale", "Detail",
                        "Higher values make the puffs smaller and more particle-like.",
                        rendererSettings.mistScale, 2.0f, 30.0f);

                    SliderFloatField(layout, "##mistSpeed", "Wind speed",
                        "How fast the fog drifts upward.",
                        rendererSettings.mistSpeed, 0.0f, 1.0f);

                    SliderFloatField(layout, "##mistDepth", "Thickness",
                        "How deep the fog volume is. Thicker fog is lit more unevenly from top to bottom.",
                        rendererSettings.mistDepth, 0.05f, 0.5f);

                    layout.AddHeader("Lighting");

                    SliderFloatField(layout, "##mistLight", "Light scatter",
                        "How strongly the saber and the pressed keys light up the fog.",
                        rendererSettings.mistLightStrength, 0.0f, 4.0f);

                    SliderFloatField(layout, "##mistLightReach", "Key light reach",
                        "How far up the screen the light of a pressed key reaches into the fog.",
                        rendererSettings.mistLightReach, 0.02f, 0.5f);

                    SliderFloatField(layout, "##mistAmbient", "Ambient glow",
                        "A faint saber-coloured glow so the fog stays visible without any light.",
                        rendererSettings.mistAmbient, 0.0f, 0.5f);

                    layout.AddHeader("Performance");

                    SliderFloatField(layout, "##mistHeight", "Height",
                        "How far the fog reaches up the screen. Lower values are faster.",
                        rendererSettings.mistHeight, 0.1f, 1.0f);

                    SliderIntField(layout, "##mistSteps", "Quality",
                        "Ray march steps per pixel. Lower values are faster but grainier.",
                        rendererSettings.mistSteps, 6, 32);
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Particle system"))
        {
            {
                ForumLayout layout("##particles");
                auto& p = rendererSettings.particleSettings;

                layout.AddHeader("Particles");

                layout.AddField("##enableParticles", "Enable particles",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.particlesEnabled);
                    });

                if (rendererSettings.particlesEnabled)
                {
                    layout.AddHeader("Appearance");

                    SliderFloatField(layout, "##particleBrightness", "Particle brightness", nullptr,
                        p.brightness, 1.0f, 20.0f);

                    layout.AddHeader("Behaviour");

                    SliderIntField(layout, "##particleEmission", "Emission",
                        "Controls how many particles are emitted (spawned) at once. Default is 12. Higher values are not recommended on Black MIDIs.",
                        p.emission, 1, 50);

                    SliderFloatField(layout, "##particleSpawnRate", "Spawn rate",
                        "Controls how frequently particle bursts are emitted. Default is 0.05. Higher values are not recommended on Black MIDIs.",
                        p.spawnRate, 0.01f, 1.0f);

                    SliderFloatField(layout, "##particleInitialSpeed", "Initial speed",
                        "Controls how fast particles fly out from the key on emission.",
                        p.initialSpeed, 0.1f, 3.0f);

                    SliderFloatField(layout, "##particleLifetime", "Lifetime",
                        "Controls how long particles survive before fading out.",
                        p.lifetime, 0.2f, 5.0f);

                    SliderFloatField(layout, "##particleLifetimeVar", "Lifetime Variance",
                        "Controls the variance of particle lifetimes. Default is 0.5. 0 = Same lifetime for all particles, 1 = random lifetime between 0 and current lifetime",
                        p.lifetimeVariance, 0.0f, 1.0f);

                    layout.AddHeader("Dynamics & Forces");

                    SliderFloatField(layout, "##particleGravity", "Gravity",
                        "Pulls particles downward over their lifetime. Negative values make them float upward.",
                        p.gravityFactor, -2.0f, 2.0f);

                    SliderFloatField(layout, "##particleDrag", "Drag",
                        "Bleeds off particle velocity over time. Higher values make particles slow down and stop faster.",
                        p.drag, 0.0f, 3.0f);

                    SliderFloatField(layout, "##particleTurbulence", "Turbulence",
                        "Adds a wobbling sideways drift to each particle's path. 0 is a straight arc.",
                        p.turbulence, 0.0f, 2.0f);

                    SliderFloatField(layout, "##particleTurbulenceVariance", "Turbulence variation",
                        "Controls how much the wobble differs from one particle to the next. 0 makes every particle wobble identically; higher values spread them out.",
                        p.turbulenceVariance, 0.0f, 1.0f);

                    SliderFloatField(layout, "##particleSwirlStrength", "Swirl strength",
                        "Pulls particles through ambient swirling eddies as they fly, rather than a straight or purely turbulent path. 0 disables it.",
                        p.swirlStrength, 0.0f, 3.0f);

                    if (p.swirlStrength > 0.0f)
                    {
                        SliderFloatField(layout, "##particleSwirlScale", "Swirl scale",
                            "Spatial frequency of the swirl field. Lower values make broad, sweeping eddies; higher values make tight, small ones.",
                            p.swirlScale, 1.0f, 40.0f);

                        SliderFloatField(layout, "##particleSwirlSpeed", "Swirl speed",
                            "How fast the vortex field itself drifts and evolves over time. 0 freezes it into a static field.",
                            p.swirlSpeed, 0.0f, 2.0f);
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Flares"))
        {
            {
                ForumLayout layout("##flares");

                layout.AddField("##enableFlares", "Enable flares",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.flaresEnabled);
                    });

                SliderFloatField(layout, "##flareHeight", "Height", nullptr,
                    rendererSettings.flareHeight, 0.1f, 0.9f);

                SliderFloatField(layout, "##flareBrightness", "Brightness", nullptr,
                    rendererSettings.flareBrightness, 0.1f, 3.0f);

                SliderFloatField(layout, "##flareFadeDuration", "Fade duration", nullptr,
                    rendererSettings.flareFadeDuration, 0.0f, 1.0f);
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererEnhanced::ResetSettings()
{
    rendererSettings = EnhancedRendererSettings::Default();

    SetupUniforms();
}

glm::mat4 MIDIRendererEnhanced::GetViewMatrixFromEuler()
{
    // Convert Euler angles to a rotation matrix
    glm::mat4 rotation = glm::mat4(1.0f);
    rotation = glm::rotate(rotation, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // Apply rotation to forward direction (0, 0, -1) to get camera's looking direction
    glm::vec4 forward = rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

    // Camera position is offset from keyboard along the looking direction (opposite)
    glm::vec3 camPos = keyboardPosition - glm::vec3(forward) * cameraDistance;
    glm::vec4 upVec = rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(upVec);

    glm::vec3 target = keyboardPosition;

    return glm::lookAt(camPos, target, up);
}

void MIDIRendererEnhanced::UploadNoteBuffer(size_t count)
{
    if (!notesIBO) return;

    notesIBO->Bind();

    GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
    void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, count * sizeof(RenderNote3D), mapFlags);
    if (ptr)
    {
        memcpy(ptr, renderNotes.data(), count * sizeof(RenderNote3D));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    glDrawElementsInstanced(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr,
        count
    );
}

void MIDIRendererEnhanced::UpdateMSAAFramebuffer()
{
    if (msaaFBO) glDeleteFramebuffers(1, &msaaFBO);
    if (msaaColorTexture) glDeleteTextures(1, &msaaColorTexture);
    if (msaaDepthRBO) glDeleteRenderbuffers(1, &msaaDepthRBO);

    msaaFBO = 0;
    msaaColorTexture = 0;
    msaaDepthRBO = 0;

    int samples = GetMSAASamples();
    if (samples <= 1) return;

    int maxSamples;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (samples > maxSamples) samples = maxSamples;

    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);

    glGenTextures(1, &msaaColorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA16F, width, height, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaColorTexture, 0);

    glGenRenderbuffers(1, &msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MIDIRendererEnhanced::ApplyRenderSize()
{
    float ss = rendererSettings.supersample;
    int maxTex; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    ss = std::min(ss, std::min(maxTex / (float)outWidth, maxTex / (float)outHeight));
    width = std::max(1, (int)std::lround(outWidth * ss));   // base-class members = render size
    height = std::max(1, (int)std::lround(outHeight * ss));

    hdrSceneFBO->Resize(width, height);
    resolvedFBO->Resize(outWidth, outHeight);
    CreateSsrTargets();

    CalcKeyPosAndWidth();

    bloomChain.clear();
    glm::vec2 mipSize(outWidth, outHeight);
    while (true)
    {
        mipSize *= 0.5f;

        int mipWidth = static_cast<int>(mipSize.x);
        int mipHeight = static_cast<int>(mipSize.y);

        if (mipWidth < 2 || mipHeight < 2) {
            break;
        }

        auto fbo = std::make_unique<Framebuffer>();
        fbo->Setup(mipWidth, mipHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        bloomChain.push_back(std::move(fbo));
    }
}

void MIDIRendererEnhanced::ResetRenderer()
{
    liveParticleCount = 0;
    for (float& timer : particleEmissionTimers)
    {
        timer = 0.0f;
    }

    for (int i = 0; i < kbIDs.size(); i++)
    {
        keyboardData[i].pressFactor = 0.0f;
        keyMetas[i].velocity = 0.0f;
        flareFade[i] = 0.0f;
    }
}

void MIDIRendererEnhanced::OnResize(int width, int height)
{
    AbstractMIDIRenderer::OnResize(width, height);
    outWidth = width;
    outHeight = height;

    if (!initialized)
    {
        float aspect = (float)width / (float)height;
        keyboardHeight = keyboardMaxZ * aspect;
        return;
    }

    ApplyRenderSize();
}

YAML::Node MIDIRendererEnhanced::GetSettings()
{
    YAML::Node node;

    YAML::Node visual;
    size_t ssaaIdx = 0;
    for (const auto& [_, val] : supersampleValues)
    {
        if (val == rendererSettings.supersample) break;
        ssaaIdx++;
    }
    visual["ssaa"] = ssaaIdx;
    visual["exposure"] = rendererSettings.exposure;

    YAML::Node keyboard;
    keyboard["keyGlowFactor"] = rendererSettings.keyGlowFactor;
    keyboard["fov"] = rendererSettings.keyboardFOV;
    keyboard["brightness"] = rendererSettings.keyboardBrightness;
    keyboard["reflectionStrength"] = rendererSettings.reflectionStrength;
    keyboard["giEnabled"] = rendererSettings.giEnabled;
    keyboard["giStrength"] = rendererSettings.giStrength;
    keyboard["giRadius"] = rendererSettings.giRadius;
    keyboard["giSaberStrength"] = rendererSettings.giSaberStrength;
    keyboard["ssrEnabled"] = rendererSettings.ssrEnabled;
    keyboard["ssrStrength"] = rendererSettings.ssrStrength;
    keyboard["ssrMaxDistance"] = rendererSettings.ssrMaxDistance;
    keyboard["ssrSteps"] = rendererSettings.ssrSteps;
    keyboard["ssrThickness"] = rendererSettings.ssrThickness;

    YAML::Node notes;
    notes["outlineGlowFactor"] = rendererSettings.noteOutlineGlowFactor;
    notes["caustic"]["strength"] = rendererSettings.causticStrength;
    notes["caustic"]["scale"] = rendererSettings.causticScale;
    notes["caustic"]["speed"] = rendererSettings.causticSpeed;
    notes["hsv"]["shiftEnabled"] = rendererSettings.hsvShiftEnabled;
    notes["hsv"]["shiftStrength"] = rendererSettings.hsvShiftStrength;
    notes["hsv"]["shifts"] = Utils::Vec3ToNode(rendererSettings.hsvShifts);

    YAML::Node saber;
    auto saberColor = rendererSettings.saberColor;
    saber["color"] = Utils::EncodeColor(ImVec4(
        saberColor.x,
        saberColor.y,
        saberColor.z,
        1.0
    ));
    saber["brightness"] = rendererSettings.saberBrightness;

    YAML::Node mist;
    mist["enabled"] = rendererSettings.mistEnabled;
    mist["density"] = rendererSettings.mistOpacity;
    mist["speed"] = rendererSettings.mistSpeed;
    mist["scale"] = rendererSettings.mistScale;
    mist["depth"] = rendererSettings.mistDepth;
    mist["height"] = rendererSettings.mistHeight;
    mist["steps"] = rendererSettings.mistSteps;
    mist["lightStrength"] = rendererSettings.mistLightStrength;
    mist["lightReach"] = rendererSettings.mistLightReach;
    mist["ambient"] = rendererSettings.mistAmbient;

    YAML::Node particles;
    particles["enabled"] = rendererSettings.particlesEnabled;

    particles["brightness"] = rendererSettings.particleSettings.brightness;
    particles["emission"] = rendererSettings.particleSettings.emission;
    particles["spawnRate"] = rendererSettings.particleSettings.spawnRate;
    particles["gravityFactor"] = rendererSettings.particleSettings.gravityFactor;
    particles["drag"] = rendererSettings.particleSettings.drag;
    particles["initialSpeed"] = rendererSettings.particleSettings.initialSpeed;
    particles["lifetime"] = rendererSettings.particleSettings.lifetime;
    particles["turbulence"] = rendererSettings.particleSettings.turbulence;
    particles["turbulenceVariance"] = rendererSettings.particleSettings.turbulenceVariance;
    particles["swirlStrength"] = rendererSettings.particleSettings.swirlStrength;
    particles["swirlScale"] = rendererSettings.particleSettings.swirlScale;
    particles["swirlSpeed"] = rendererSettings.particleSettings.swirlSpeed;

    YAML::Node flares;
    flares["enabled"] = rendererSettings.flaresEnabled;
    flares["height"] = rendererSettings.flareHeight;
    flares["brightness"] = rendererSettings.flareBrightness;
    flares["fadeDuration"] = rendererSettings.flareFadeDuration;

    node["visual"] = visual;
    node["notes"] = notes;
    node["keyboard"] = keyboard;
    node["saber"] = saber;
    node["mist"] = mist;
    node["particles"] = particles;
    node["flares"] = flares;

    return node;
}

void MIDIRendererEnhanced::LoadSettings(const YAML::Node& node)
{
    if (!node) return;

    if (node["visual"])
    {
        auto visual = node["visual"];
        int ssaaVal = 0;
        LoadValue(visual, "ssaa", ssaaVal);
        ssaaVal = std::clamp<int>(ssaaVal, 0, supersampleValues.size() - 1);
        rendererSettings.supersample = std::get<1>(supersampleValues[ssaaVal]);
        LoadValue(visual, "exposure", rendererSettings.exposure);
    }

    if (node["keyboard"])
    {
        auto keyboard = node["keyboard"];
        LoadValue(keyboard, "keyGlowFactor", rendererSettings.keyGlowFactor);
        LoadValue(keyboard, "fov", rendererSettings.keyboardFOV);
        LoadValue(keyboard, "brightness", rendererSettings.keyboardBrightness);
        LoadValue(keyboard, "reflectionStrength", rendererSettings.reflectionStrength);
        LoadValue(keyboard, "giEnabled", rendererSettings.giEnabled);
        LoadValue(keyboard, "giStrength", rendererSettings.giStrength);
        LoadValue(keyboard, "giRadius", rendererSettings.giRadius);
        LoadValue(keyboard, "giSaberStrength", rendererSettings.giSaberStrength);
        LoadValue(keyboard, "ssrEnabled", rendererSettings.ssrEnabled);
        LoadValue(keyboard, "ssrStrength", rendererSettings.ssrStrength);
        LoadValue(keyboard, "ssrMaxDistance", rendererSettings.ssrMaxDistance);
        LoadValue(keyboard, "ssrSteps", rendererSettings.ssrSteps);
        LoadValue(keyboard, "ssrThickness", rendererSettings.ssrThickness);
    }

    if (node["notes"])
    {
        auto notes = node["notes"];
        LoadValue(notes, "outlineGlowFactor", rendererSettings.noteOutlineGlowFactor);

        if (notes["caustic"])
        {
            auto caustic = notes["caustic"];
            LoadValue(caustic, "strength", rendererSettings.causticStrength);
            LoadValue(caustic, "scale", rendererSettings.causticScale);
            LoadValue(caustic, "speed", rendererSettings.causticSpeed);
        }

        if (notes["hsv"])
        {
            auto hsv = notes["hsv"];
            LoadValue(hsv, "shiftEnabled", rendererSettings.hsvShiftEnabled);
            LoadValue(hsv, "shiftStrength", rendererSettings.hsvShiftStrength);
            if (hsv["shifts"])
            {
                rendererSettings.hsvShifts = Utils::NodeToVec3(hsv["shifts"]);
            }
        }
    }

    if (node["saber"])
    {
        auto saber = node["saber"];
        LoadValue(saber, "brightness", rendererSettings.saberBrightness);
        if (saber["color"])
        {
            std::variant<std::string, uint32_t> colorInput;
            try
            {
                colorInput = saber["color"].as<std::string>();
            }
            catch (...)
            {
                colorInput = saber["color"].as<uint32_t>();
            }

            auto parsedColor = Utils::ParseColor(colorInput, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (parsedColor)
            {
                rendererSettings.saberColor = glm::vec3(parsedColor->x, parsedColor->y, parsedColor->z);

            }
        }
    }

    if (node["mist"])
    {
        auto mist = node["mist"];
        LoadValue(mist, "enabled", rendererSettings.mistEnabled);
        LoadValue(mist, "density", rendererSettings.mistOpacity);
        LoadValue(mist, "speed", rendererSettings.mistSpeed);
        LoadValue(mist, "scale", rendererSettings.mistScale);
        LoadValue(mist, "depth", rendererSettings.mistDepth);
        LoadValue(mist, "height", rendererSettings.mistHeight);
        LoadValue(mist, "steps", rendererSettings.mistSteps);
        LoadValue(mist, "lightStrength", rendererSettings.mistLightStrength);
        LoadValue(mist, "lightReach", rendererSettings.mistLightReach);
        LoadValue(mist, "ambient", rendererSettings.mistAmbient);
    }

    if (node["particles"])
    {
        auto particles = node["particles"];
        LoadValue(particles, "enabled", rendererSettings.particlesEnabled);
        LoadValue(particles, "brightness", rendererSettings.particleSettings.brightness);
        LoadValue(particles, "emission", rendererSettings.particleSettings.emission);
        LoadValue(particles, "spawnRate", rendererSettings.particleSettings.spawnRate);
        LoadValue(particles, "gravityFactor", rendererSettings.particleSettings.gravityFactor);
        LoadValue(particles, "drag", rendererSettings.particleSettings.drag);
        LoadValue(particles, "initialSpeed", rendererSettings.particleSettings.initialSpeed);
        LoadValue(particles, "lifetime", rendererSettings.particleSettings.lifetime);
        LoadValue(particles, "turbulence", rendererSettings.particleSettings.turbulence);
        LoadValue(particles, "turbulenceVariance", rendererSettings.particleSettings.turbulenceVariance);
        LoadValue(particles, "swirlStrength", rendererSettings.particleSettings.swirlStrength);
        LoadValue(particles, "swirlScale", rendererSettings.particleSettings.swirlScale);
        LoadValue(particles, "swirlSpeed", rendererSettings.particleSettings.swirlSpeed);
    }

    if (node["flares"])
    {
        auto flare = node["flares"];
        LoadValue(flare, "enabled", rendererSettings.flaresEnabled);
        LoadValue(flare, "height", rendererSettings.flareHeight);
        LoadValue(flare, "fadeDuration", rendererSettings.flareFadeDuration);
        LoadValue(flare, "brightness", rendererSettings.flareBrightness);
    }

    SetupUniforms();
    if (initialized && rendererSettings.supersample != 1.0) ApplyRenderSize();
}