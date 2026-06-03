#version 330

layout (location = 0) in vec2 aPos;

// instance data
layout (location = 1) in float kLeft;
layout (location = 2) in float kRight;
layout (location = 3) in uint kMeta;

out vec2 uv;
flat out uint meta;

uniform float kbHeight;
uniform float kbWhiteHeight;
uniform float kbBlackHeight;

out float kWidth;

void main()
{
	bool black   = (kMeta & (1u << 25)) != 0u;

	float x = mix(kLeft, kRight, aPos.x);
	float y = kbHeight - aPos.y * (black ? kbBlackHeight : kbWhiteHeight);

	gl_Position = vec4(x * 2.0f - 1.0f, (y * 2.0f - 1.0f), 0.0f, 1.0f);
	uv = aPos;
	meta = kMeta;
	kWidth = kRight - kLeft;
}