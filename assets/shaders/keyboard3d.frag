#version 330 core

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
}