#version 330 core

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
}
