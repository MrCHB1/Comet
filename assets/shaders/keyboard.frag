#version 330

uniform sampler2D blackKey;
uniform sampler2D whiteKey;
uniform sampler2D blackKeyPressed;
uniform sampler2D whiteKeyPressed;
uniform sampler2D blackKeyMask;
uniform sampler2D whiteKeyMask;
uniform sampler2D blackKeyPressedMask;
uniform sampler2D whiteKeyPressedMask;

uniform vec3 barColor;

flat in uint meta;
in vec2 uv;
in float kWidth;

uniform float whiteKeyBorderPixelWidth; // how much pixels the border should take up when rendering
uniform float borderPercentageNorm; // 0.0-1.0 of how much the border takes up the texure

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

	float uvPerPixel = fwidth(uv.x);
    float keyWidthPixels = 1.0 / uvPerPixel;

	float actualBorderPx = min(whiteKeyBorderPixelWidth, keyWidthPixels * 0.5);

	float pixelX = uv.x * keyWidthPixels;
	float adjustedU = uv.x;

	if (pixelX < actualBorderPx)
	{
		// Left Border (maps to 0.0 -> borderPercentageNorm)
        adjustedU = mix(0.0, borderPercentageNorm, pixelX / actualBorderPx);
	}
	else if (pixelX > keyWidthPixels - actualBorderPx)
	{
		float distFromRight = keyWidthPixels - pixelX;
        adjustedU = mix(1.0, 1.0 - borderPercentageNorm, distFromRight / actualBorderPx);
	}
	else
	{
		float middlePixelWidth = keyWidthPixels - (2.0 * actualBorderPx);
        float distFromLeftBorder = pixelX - actualBorderPx;
        adjustedU = mix(borderPercentageNorm, 1.0 - borderPercentageNorm, distFromLeftBorder / middlePixelWidth);
	}

	vec2 sliceUV = vec2(adjustedU, uv.y);

	vec3 texColor;

	if (!pressed)
	{
		if (black)
		{
			vec2 blackKeyMaskRG = texture(blackKeyMask, uv).rg;
			texColor = texture(blackKey, uv).rgb;
			vec3 texColorBar = texColor * barColor;
			texColor = mix(texColor, texColorBar, blackKeyMaskRG.g);
		}
		else
		{
			vec2 whiteKeyMaskRG = texture(whiteKeyMask, uv).rg;
			texColor = texture(whiteKey, sliceUV).rgb;
			vec3 texColorBar = texColor * barColor;
			texColor = mix(texColor, texColorBar, whiteKeyMaskRG.g);
		}
	}
	else
	{
		if (black)
		{
			vec2 blackKeyPressedMaskRG = texture(blackKeyPressedMask, uv).rg;

			texColor = texture(blackKeyPressed, uv).rgb;
			vec3 texColorBar = texColor * barColor;
			vec3 texColorNote = texColor * color;
			texColor = mix(texColor, texColorBar, blackKeyPressedMaskRG.g);
			texColor = mix(texColor, texColorNote, blackKeyPressedMaskRG.r);
		}
		else
		{
			vec2 whiteKeyPressedMaskRG = texture(whiteKeyPressedMask, uv).rg;

			texColor = texture(whiteKeyPressed, sliceUV).rgb;
			vec3 texColorBar = texColor * barColor;
			vec3 texColorNote = texColor * color;
			texColor = mix(texColor, texColorBar, whiteKeyPressedMaskRG.g);
			texColor = mix(texColor, texColorNote, whiteKeyPressedMaskRG.r);
		}
	}

	fragColor = vec4(texColor, 1.0);
}