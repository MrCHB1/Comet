#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 texcoord;
void main()
{
	texcoord = aPos;
	gl_Position = vec4(aPos * 2.0f - 1.0f, 0.0f, 1.0f);
}
