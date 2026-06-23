#version 330 core
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
}