#version 330

uniform sampler2D blackKey;
uniform sampler2D whiteKey;
uniform sampler2D blackKeyPressed;
uniform sampler2D whiteKeyPressed;

flat in uint meta;
in vec2 uv;

out vec4 fragColor;

void main()
{
	vec3 color = vec3(meta & 0xFFFFFFu) / 255.0;

	bool pressed = (meta & (1u << 24)) != 0u;
    bool black   = (meta & (1u << 25)) != 0u;

	vec3 texColor;
	if (!pressed)
	{
		texColor = texture(black ? blackKey : whiteKey, uv).rgb;
	}
	else
	{
		texColor = texture(black ? blackKeyPressed : whiteKeyPressed, uv).rgb * color;
	}

	fragColor = vec4(texColor, 1.0);
}