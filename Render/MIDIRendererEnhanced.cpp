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
    localPos.y *= isBlack ? 0.035 : 0.0175; // Much shorter Y height
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
    
    // Elevate black keys slightly so they sit definitively on top of white keys
    if (isBlack) {
        worldPos.y += 0.001 * keyboardThickness; 
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

uniform float keyGlowFactor;
uniform float keyboardBrightness;
uniform vec3 cameraPos;
uniform float animTime;
uniform bool noteHsvShiftEnabled;
uniform vec3 noteHsvShifts;
uniform float noteHsvShiftStrength;

out vec4 fragColor;

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
        vec2 uv = (LocalUnitPos.xz + vec2(hash(vec2(KeyPos, 0.0f)) * 3.248f, 0.0f)) * vec2(4.0f, 12.0f);

        float n1 = noise(uv + vec2(animTime * 0.6f, 0.0f));
        float n2 = noise(uv * 2.0f - vec2(0.0f, animTime * 0.8f));

        vec2 warpedUV = uv + vec2(n1, n2);
        float warp = noise(warpedUV);

        vec3 surfaceColor = mix(noteColor, noteColor * 1.8f, warp);

        float uvWidth = fwidth(LocalUnitPos.x);
        float uvHeight = fwidth(LocalUnitPos.z);
        const float outlinePx = 2.5f;

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
        vec3 baseColor = isBlack ? vec3(0.12) : vec3(1.0f);
        vec3 normal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));

        vec3 lightDir = normalize(vec3(0.0, 0.9, 0.3));
        float diffuse = max(dot(normal, lightDir), 0.0);

        if (diffuse < 0.001) {
            diffuse = max(dot(-normal, lightDir), 0.0);
        }

        float ambient = 0.75;

        // specular lol
        vec3 viewDir = normalize(cameraPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specular = pow(
            max(dot(viewDir, reflectDir), 0.0),
            isBlack ? 128.0 : 96.0
        );
        float specStrength = isBlack ? 0.7 : 0.4;

        fragColor = vec4((baseColor * (ambient + (diffuse * 0.75)) + specular * specStrength) * keyboardBrightness, 1.0f);
    }
})";

    constexpr const char* mist3dvert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 texcoord;
void main()
{
	texcoord = aPos;
	gl_Position = vec4(aPos * 2.0f - 1.0f, 0.0f, 1.0f);
})";

    constexpr const char* mist3dfrag = R"(#version 330

out vec4 fragColor;
in vec2 texcoord;

uniform vec3 mistColor;
uniform float mistOpacity;
uniform float mistSpeed;
uniform float mistScale;

uniform float animTime;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 0.0;
    
    // 4 octaves of noise for detail
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main()
{
    vec2 uv = texcoord * mistScale;
    uv.y -= animTime * mistSpeed;

    vec2 swirlUV = texcoord * (mistScale * 1.5f);
    swirlUV.x += animTime * (mistSpeed * 0.5f);
    swirlUV.y -= animTime * (mistSpeed * 0.8f);

    float n1 = fbm(uv);
    float n2 = fbm(swirlUV + vec2(n1));

    float edgeFade = smoothstep(1.0f, 0.0f, min(texcoord.y * 2.0f, 1.0f));

    float finalAlpha = n2 * mistOpacity * edgeFade;
	fragColor = vec4(mistColor * finalAlpha, finalAlpha);
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
	float x = mix(nLeft, nRight, aPos.x);
	float y = mix(nStart, nEnd, aPos.y);
	y = y * (1.0f - kbHeight) + kbHeight;

	gl_Position = vec4(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f);
	uv = aPos;
	color = nColor;
	fragPos = vec2(x, y);
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

    FragColor = vec4(mapped, 1.0);
})";
#pragma endregion
}

MIDIRendererEnhanced::MIDIRendererEnhanced(MIDIApp* app) : AbstractMIDIRenderer(app)
{
    keyboardData.fill(RenderKeyboardKey3D());
    keyMetas.fill(KeyboardMeta3D());
}

MIDIRendererEnhanced::~MIDIRendererEnhanced() = default;

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

    glm::vec2 mipSize(width, height);
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
#pragma endregion

#pragma region uniforms setup
    SetupUniforms();
#pragma endregion

    initialized = true;
    CalcKeyPosAndWidth();
    UpdateMSAAFramebuffer();
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
        keyboardProgram->SetFloat("keyboardBrightness", 1.0f);
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

void MIDIRendererEnhanced::UpdateKeyboardInstance(double deltaTime)
{
    bool needsUpload = false;

    // cap the dt so physics does not explode lol
    float dt = static_cast<float>(std::min(deltaTime, 0.033));

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

void MIDIRendererEnhanced::RenderKeyboard()
{
    ShaderBind kbBind(*keyboardProgram);

    // calculate required camera distance based on keyboard height and FOV
    // this ensures the keyboard spans the appropriate portion of the viewport
    float aspect = (float)width / (float)height;
    float verticalFOVRad = glm::radians(rendererSettings.keyboardFOV);

    cameraDistance = 1.0f / (2.0f * tan(verticalFOVRad / 2.0f) * aspect);

    float visibleHeight = 2.0f * cameraDistance * tan(verticalFOVRad / 2.0f);
    float keyDepthReach = keyboardHeight * 0.55f; // 2.2 * 0.25, matches the shader's actual multiplier
    keyboardZOffset = (visibleHeight / 2.0f) - keyDepthReach;
    keyboardZOffset += keyboardBottomFill;

    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);

    glm::mat4 view = GetViewMatrixFromEuler();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

    keyboardProgram->SetMat4("projection", projection);
    keyboardProgram->SetMat4("view", view);
    keyboardProgram->SetFloat("keyboardZOffset", keyboardZOffset);
    keyboardProgram->SetFloat("keyboardHeight", keyboardHeight);
    keyboardProgram->SetVec3("cameraPos", cameraPos);
    keyboardProgram->SetFloat("animTime", app->GetTimer()->Elapsed());


    {
        VertexArrayBind whiteBind(*whiteKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::WhiteKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawWhiteKeys);
    }

    {
        VertexArrayBind blackBind(*blackKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::BlackKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawBlackKeys);
    }
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

    notesProgram->SetFloat("kbHeight", keyboardHeight * ((float)width / (float)height) / (16.0 / 9.0));
    notesProgram->SetFloat("animTime", static_cast<float>(f.playbackSeconds));

    const double kbNorm = (double)keyboardHeight * ((double)width / height) / (16.0 / 9.0);
    const double noteAreaPx = (double)height * (1.0 - kbNorm);
    CollectNotes(f, { .noteAreaPx = noteAreaPx, .yBase = noteAreaPx },
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

void MIDIRendererEnhanced::RenderMist()
{
    mistQuad->SetTransform({ glm::vec3(0.0f, keyboardHeight, 0.0f), glm::vec2(1.0f, 1.0f - keyboardHeight) });
    mistQuad->Draw();

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetFloat("animTime", app->GetTimer()->Elapsed());
    }
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
        p.position = glm::vec3(spawnX + ((rand() / (float)RAND_MAX)) * keyWidth[keyID], keyboardHeight * ((float)width / (float)height) / (16.0 / 9.0), 0.0f);

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

    flaresProgram->SetFloat("kbHeight", keyboardHeight * ((float)width / (float)height) / (16.0 / 9.0));
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

    int samples = GetMSAASamples();

    if (samples > 1)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
        glEnable(GL_MULTISAMPLE);
    }
    else
    {
        hdrSceneFBO->Bind();
        glDisable(GL_MULTISAMPLE);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    RenderNotes();

    if (rendererSettings.mistEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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

    if (samples > 1)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrSceneFBO->GetID());
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        hdrSceneFBO->Unbind();
    }

    glDisable(GL_DEPTH_TEST);

    {
        ShaderBind downsampleBind(*downsampleShader);
        downsampleShader->SetInt("srcTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrSceneFBO->GetSceneTexture());

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
    glViewport(0, 0, width, height);

    {
        ShaderBind compositeBind(*compositeShader);
        compositeShader->SetInt("sceneTex", 0);
        compositeShader->SetInt("bloomTex", 1);
        compositeShader->SetFloat("exposure", rendererSettings.exposure);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrSceneFBO->GetSceneTexture());

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

                layout.AddField("##msaa", "Anti-aliasing",
                    [&]()
                    {
                        MSAASetting previousMSAA = rendererSettings.msaa;

                        IMGUI_RADIO_BUTTON("None", rendererSettings.msaa, MSAASetting::None);
                        IMGUI_RADIO_BUTTON("Low (2x2)", rendererSettings.msaa, MSAASetting::AA2x2);
                        IMGUI_RADIO_BUTTON("Medium (4x4)", rendererSettings.msaa, MSAASetting::AA4x4);
                        IMGUI_RADIO_BUTTON("High (6x6)", rendererSettings.msaa, MSAASetting::AA6x6);

                        if (previousMSAA != rendererSettings.msaa)
                            UpdateMSAAFramebuffer();
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
                layout.AddHeader("Mist");

                layout.AddField("##enableMist", "Enable mist",
                    [&]()
                    {
                        ImGui::Checkbox("", &rendererSettings.mistEnabled);
                    });
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

    {
        ShaderBind notesBind(*notesProgram);
        notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
        notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind keyboardBind(*keyboardProgram);
        keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
    }

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
    }
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
    this->width = width;
    this->height = height;

    if (!initialized)
    {
        float aspect = (float)width / (float)height;
        keyboardHeight = keyboardMaxZ * aspect;
        return;
    }
    CalcKeyPosAndWidth();

    // regenerate bloom stuff
    hdrSceneFBO->Resize(width, height);
    bloomChain.clear();

    glm::vec2 mipSize(width, height);
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

    // update msaa buffers
    UpdateMSAAFramebuffer();
}

YAML::Node MIDIRendererEnhanced::GetSettings()
{
    YAML::Node node;

    YAML::Node visual;
    visual["msaa"] = static_cast<int>(rendererSettings.msaa);
    visual["exposure"] = rendererSettings.exposure;

    YAML::Node keyboard;
    keyboard["keyGlowFactor"] = rendererSettings.keyGlowFactor;
    keyboard["fov"] = rendererSettings.keyboardFOV;
    keyboard["brightness"] = rendererSettings.keyboardBrightness;

    YAML::Node notes;
    notes["outlineGlowFactor"] = rendererSettings.noteOutlineGlowFactor;
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
        int msaaVal = static_cast<int>(rendererSettings.msaa);
        LoadValue(visual, "msaa", msaaVal);
        rendererSettings.msaa = static_cast<decltype(rendererSettings.msaa)>(msaaVal);
        LoadValue(visual, "exposure", rendererSettings.exposure);
    }

    if (node["keyboard"])
    {
        auto keyboard = node["keyboard"];
        LoadValue(keyboard, "keyGlowFactor", rendererSettings.keyGlowFactor);
        LoadValue(keyboard, "fov", rendererSettings.keyboardFOV);
        LoadValue(keyboard, "brightness", rendererSettings.keyboardBrightness);
    }

    if (node["notes"])
    {
        auto notes = node["notes"];
        LoadValue(notes, "outlineGlowFactor", rendererSettings.noteOutlineGlowFactor);

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

    if (node["flare"])
    {
        auto flare = node["flare"];
        LoadValue(flare, "enabled", rendererSettings.flaresEnabled);
        LoadValue(flare, "height", rendererSettings.flareHeight);
        LoadValue(flare, "fadeDuration", rendererSettings.flareFadeDuration);
        LoadValue(flare, "brightness", rendererSettings.flareBrightness);
    }

    SetupUniforms();
    if (rendererSettings.msaa != MSAASetting::None) UpdateMSAAFramebuffer();
}