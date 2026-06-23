#version 330 core
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
}