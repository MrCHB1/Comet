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
	vec3 color = vec3(
		float((meta & 0xFF0000u) >> 16u), 
		float((meta & 0xFF00u) >> 8u),
		float(meta & 0xFFu)
	) / 255.0f;

	bool pressed = (meta & (1u << 24)) != 0u;
    bool black   = (meta & (1u << 25)) != 0u;

	vec3 texColor;
	if (!pressed)
	{
		if (black)
			texColor = texture(blackKey, uv).rgb;
		else
			texColor = texture(whiteKey, uv).rgb;
	}
	else
	{
		if (black)
			texColor = texture(blackKeyPressed, uv).rgb * color;
		else
			texColor = texture(whiteKeyPressed, uv).rgb * color;
	}

	fragColor = vec4(texColor, 1.0);
}