#version 330 core

layout (location = 0) in vec2 aPos;       
layout (location = 1) in vec2 aTexCoords; 

out vec2 texcoord;

void main()
{
    texcoord = aTexCoords;
    gl_Position = vec4(aPos.x * 2.0 - 1.0, aPos.y * 2.0 - 1.0, 0.0, 1.0); 
}