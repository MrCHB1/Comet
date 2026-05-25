#version 330

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

uniform float kbHeight;

void main()
{
	float x = mix(nLeft, nRight, aPos.x);
	float y = mix(nStart, nEnd, aPos.y);
	y = y * (1.0f - kbHeight) + kbHeight;

	gl_Position = vec4(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f);
	uv = aPos;
	color = nColor;
	noteHeight = nEnd - nStart;
}