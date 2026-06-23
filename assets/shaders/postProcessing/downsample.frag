#version 330 core
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
}