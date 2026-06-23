#version 330 core
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
}