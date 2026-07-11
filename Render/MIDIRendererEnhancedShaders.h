#pragma once

#if defined(ENHANCED_SHADERS)

inline constexpr const char* keyboard3dvert = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aLeft;
layout (location = 2) in float aRight;
layout (location = 3) in float aPressFactor;
layout (location = 4) in uint aMeta;

uniform mat4 projection;
uniform mat4 view;
uniform float keyboardZOffset;

out vec3 FragPos;
out vec2 TexCoord;
out float pressFactor;
flat out uint meta;

out vec3 LocalUnitPos;
out float KeyPos;

const float keyboardThickness = 0.20f;
const float keyboardHeight = 0.3f;

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
    localPos.z *= keyboardHeight;

    // 3. Fix Dimensions: Drastically reduce height to look like standard keys
    localPos.y *= isBlack ? 0.035 : 0.0175; // Much shorter Y height
    localPos.z *= isBlack ? 0.15 : 0.25; // Depth towards the camera

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

inline constexpr const char* keyboard3dfrag = R"(#version 330 core

in vec3 FragPos;
in vec2 TexCoord;
in float pressFactor;
flat in uint meta;

in vec3 LocalUnitPos;
in float KeyPos;

uniform float keyGlowFactor;
uniform vec3 cameraPos;
uniform float animTime;

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

        float ambient = 0.25;

        // specular lol
        vec3 viewDir = normalize(cameraPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specular = pow(
            max(dot(viewDir, reflectDir), 0.0),
            isBlack ? 128.0 : 96.0
        );
        float specStrength = isBlack ? 0.7 : 0.4;

        fragColor = vec4(baseColor * (ambient + (diffuse * 0.75)) + specular * specStrength, 1.0f);
    }
})";

inline constexpr const char* mist3dvert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 texcoord;
void main()
{
	texcoord = aPos;
	gl_Position = vec4(aPos * 2.0f - 1.0f, 0.0f, 1.0f);
})";

inline constexpr const char* mist3dfrag = R"(#version 330

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

inline constexpr const char* notes3dvert = R"(#version 330

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

inline constexpr const char* notes3dfrag = R"(#version 330

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

inline constexpr const char* particle3dvert = R"(#version 330 core
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

inline constexpr const char* saber3dvert = R"(#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
})";

inline constexpr const char* saber3dfrag = R"(#version 330 core
out vec4 FragColor;

uniform vec3 saberColor;

void main() {
    FragColor = vec4(saberColor, 1.0);
})";

inline constexpr const char* fullscreenvert = R"(#version 330 core

layout (location = 0) in vec2 aPos;       
layout (location = 1) in vec2 aTexCoords; 

out vec2 texcoord;

void main()
{
    texcoord = aTexCoords;
    gl_Position = vec4(aPos.x * 2.0 - 1.0, aPos.y * 2.0 - 1.0, 0.0, 1.0); 
})";

inline constexpr const char* downsamplefrag = R"(#version 330 core
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

inline constexpr const char* upsamplefrag = R"(#version 330 core
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

inline constexpr const char* compositefrag = R"(#version 330 core
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

#endif