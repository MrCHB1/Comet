#version 330

in vec2 uv;
flat in uint color;
in float noteHeight;

out vec4 fragColor;

uniform sampler2D note;
uniform sampler2D noteBlack;
uniform sampler2D noteEdge;
uniform float noteBorderWidth;

void main()
{
	bool isBlack = (color & 0x1000000u) != 0u;
	vec3 nCol = vec3(
		float((color & 0xFF0000u) >> 16u), 
		float((color & 0xFF00u) >> 8u),
		float(color & 0xFFu)
	) / 255.0f;

	vec3 noteTex;
	if (isBlack)
	{
		noteTex = texture(noteBlack, uv).rgb;
	}
	else
	{
		noteTex = texture(note, uv).rgb;
	}

	float uvBorderThickness = noteBorderWidth / noteHeight;

	vec4 edgeTex = vec4(0.0);

	if (uv.y < uvBorderThickness)
	{
		float normY = uv.y / uvBorderThickness;
		edgeTex = texture(noteEdge, vec2(uv.x, normY));
	}
	else if (uv.y > (1.0 - uvBorderThickness))
    {
        float normY = (1.0 - uv.y) / uvBorderThickness;
        edgeTex = texture(noteEdge, vec2(uv.x, normY));
    }

	vec3 finalColor = mix(noteTex, edgeTex.rgb, edgeTex.a) * nCol;

	fragColor = vec4(finalColor, 1.0f);
}