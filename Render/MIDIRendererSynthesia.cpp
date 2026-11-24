#include <memory>
#include <algorithm>

#include "RenderEngine/Shaders.h"
#include "RenderEngine/Buffers.h"
#include "RenderEngine/TextureArray.h"

#include "MIDIRendererSynthesia.h"
#include "Utils.h"
#include "App/MIDIApp.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"
#include "RenderView.h"
#include "App/Dialog/DialogMacros.h"

#include "Config/ConfigUtils.h"
#include "App/UI/Layout/ForumLayout.h"

namespace
{
	using namespace Config;
	using namespace Comet::UI::Layout;

	// default note colors (white keys); black keys get a darker shade of the same hue
	constexpr const std::array<uint32_t, 6> nativeSynthesiaColors = {
		0xF18484, // red
		0xFDF18C, // yellow
		0xFDC066, // orange
		0xA9E96A, // green
		0x9BBADD, // blue
		0xBE86C4, //purple
	};

	// dark-key note colors, same order as above. 
	constexpr const std::array<uint32_t, 6> nativeSynthesiaColorsDark = {
		0xEC4F4F, // red
		0xDDC918, // yellow
		0xF6871B, // orange
		0x73AE3A, // green
		0x5780B4, // blue
		0x7E6282, // purple
	};

	constexpr uint32_t DarkenColor(uint32_t c, float f)
	{
		return ((uint32_t)(((c >> 16) & 0xFF) * f) << 16) |
			((uint32_t)(((c >> 8) & 0xFF) * f) << 8) |
			(uint32_t)((c & 0xFF) * f);
	}

#pragma region Texture enums

	// Index of each source image within the shared texture array. Order must
	// match the LoadLayer() calls in Initialize().
	enum S9TextureLayer : int
	{
		S9_LAYER_BAR = 0,
		S9_LAYER_KEY_BLACK,
		S9_LAYER_KEY_BLACK_PRESSED,
		S9_LAYER_KEY_WHITE,
		S9_LAYER_KEY_WHITE_PRESSED,
		S9_LAYER_KEY_WHITE_WHOLE,
		S9_LAYER_KEY_WHITE_WHOLE_PRESSED,
		S9_LAYER_SHADOW_LARGE,
		S9_LAYER_SHADOW_UNPRESSED,
		S9_LAYER_SHADOW_PRESSED,

		S9_LAYER_COUNT
	};

	enum S8TextureLayer : int
	{
		S8_LAYER_BAR = 0,
		S8_LAYER_KEY_BLACK,
		S8_LAYER_KEY_BLACK_PRESSED,
		S8_LAYER_KEY_WHITE,
		S8_LAYER_KEY_WHITE_PRESSED,
		S8_LAYER_KEY_WHITE_WHOLE,
		S8_LAYER_KEY_WHITE_WHOLE_PRESSED,

		S8_LAYER_COUNT
	};

	enum S7TextureLayer : int
	{
		S7_LAYER_BAR = 0,
		S7_LAYER_KEY_BLACK,
		S7_LAYER_KEY_BLACK_PRESSED,
		S7_LAYER_KEY_WHITE,
		S7_LAYER_KEY_WHITE_PRESSED,
		S7_LAYER_KEY_WHITE_WHOLE,
		S7_LAYER_KEY_WHITE_WHOLE_PRESSED,

		S7_LAYER_COUNT
	};

	enum GlowTextureLayer : int
	{
		GLOW_LAYER_BAR = 0,
		GLOW_LAYER_KEY_BLACK,
		GLOW_LAYER_KEY_BLACK_PRESSED,
		GLOW_LAYER_KEY_WHITE,
		GLOW_LAYER_KEY_WHITE_PRESSED,
		GLOW_LAYER_KEY_WHITE_WHOLE,
		GLOW_LAYER_KEY_WHITE_WHOLE_PRESSED,

		GLOW_LAYER_COUNT
	};

	enum ParticleTextureLayer : int
	{
		LAYER_PARTICLE_DEBRIS = 0,
		LAYER_PARTICLE_SPARKLE,
		LAYER_PARTICLE_HAZE,
		PARTICLE_LAYER_COUNT
	};

#pragma endregion

#pragma region Shaders
	constexpr const char* rectVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 rectPos;
layout (location = 2) in vec2 rectSize;
layout (location = 3) in float rotation;
layout (location = 4) in vec2 uv0;
layout (location = 5) in vec2 uv1;
layout (location = 6) in int textureIndex;
layout (location = 7) in uint aMeta; 

flat out uint meta;
flat out int texID;

out vec2 uv;

uniform float aspect;

void main()
{
    meta = aMeta;
    texID = textureIndex;
    uv = mix(uv0, uv1, aPos);

    vec2 localPos = (aPos - vec2(0.5)) * rectSize;

    // Convert to aspect-correct space
    localPos.x *= aspect;

    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rot = mat2(c, -s,
                    s,  c);

    localPos = rot * localPos;

    // Undo aspect correction
    localPos.x /= aspect;

    vec2 pos = rectPos + rectSize * 0.5 + localPos;

    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
})";

	constexpr const char* rectFrag = R"(#version 330 core
flat in uint meta;
flat in int texID;
in vec2 uv;

out vec4 fragColor;

uniform sampler2DArray textures;

void main()
{
	bool isTexture = (meta & 0x80000000u) != 0u;
	
	vec4 rectColor = vec4(float((meta & 0xFF0000u) >> 16u) / 255.0,
		float((meta & 0xFF00u) >> 8u) / 255.0,
		float(meta & 0xFFu) / 255.0,
		float((meta & 0x7F000000u) >> 24u) / 127.0);

	if (isTexture)
		fragColor = texture(textures, vec3(uv, float(texID))) * rectColor;
	else
		fragColor = rectColor;
})";

	constexpr const char* notesVert = R"(#version 330 core
layout (location = 0) in vec2 vPos;
layout (location = 1) in vec2 aPos;
layout (location = 2) in vec2 aSize;
layout (location = 3) in uint aMeta;

out vec2 noteSize;   // quad size in px
out vec2 uv;
flat out uint meta;
flat out float cornerY; // px height of the crown / heel

uniform float kbHeight;
uniform vec2 texSizeWhite; // texels
uniform vec2 texSizeBlack;
// note space -> px
uniform vec2 noteToPx;

void main()
{
	bool isBlack = (aMeta & (1u << 24u)) != 0u;
	bool isOOB   = (aMeta & (1u << 25u)) != 0u;

	uv = vPos;
	meta = aMeta;
	cornerY = 0.0;

	vec2 origin;
	vec2 sizePx;
	if (isOOB)
	{
		origin = aPos * noteToPx;
		sizePx = aSize * noteToPx;
	}
	else
	{
		// Synthesia: note is 1px wider than its key on each side, and the whole texture is
		// scaled by that width, so crown/heel are proportional to the note width.
		vec2 tex = isBlack ? texSizeBlack : texSizeWhite;
		float widthPx = aSize.x * noteToPx.x + 2.0;
		float s = widthPx / tex.x;
		// at least crown + heel (+1px) tall; grows upwards from the note start
		float heightPx = max(aSize.y * noteToPx.y, tex.y * s + 1.0);

		origin = vec2(aPos.x * noteToPx.x - 1.0, aPos.y * noteToPx.y);
		sizePx = vec2(widthPx, heightPx);
		cornerY = tex.y * 0.5 * s;
	}
	noteSize = sizePx;

	vec2 pos = (origin + vPos * sizePx) / noteToPx;
	pos.y = pos.y * (1.0 - kbHeight) + kbHeight;
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
})";

	constexpr const char* notesFrag = R"(#version 330 core
in vec2 noteSize;
in vec2 uv;
flat in uint meta;
flat in float cornerY;

uniform sampler2D noteWhite;
uniform sampler2D noteBlack;
uniform sampler2D noteOOB;

uniform bool nativeColors;

uniform float brightnessBoost;

// Distance d from the low edge of a span of length len -> texture coord.
// Crown and heel keep their size, the middle repeats the center texel line.
// Spans shorter than crown + heel crop them instead of squashing.
float sliceAxis(float d, float len, float corner)
{
	if (d < len * 0.5) return 0.5 * min(d / corner, 1.0);
	return 1.0 - 0.5 * min((len - d) / corner, 1.0);
}

void main()
{
	vec3 noteColor = vec3(float((meta & 0xFF0000u) >> 16u),
		float((meta & 0xFF00u) >> 8u),
		float(meta & 0xFFu)) / 255.0;
	vec3 noteColor2 = noteColor * brightnessBoost;

	bool isBlack = (meta & (1u << 24u)) != 0u;
	bool isOOB   = (meta & (1u << 25u)) != 0u;
	bool isRight = (meta & (1u << 26u)) != 0u;

	if (isBlack && !nativeColors)
	{
		noteColor *= 0.7;
		noteColor2 *= 0.7;
	}

	vec4 noteTexColor;

	if (!isOOB)
	{
		vec2 texUV;
		texUV.x = uv.x;
		// texture v=0 is the note's top edge
		texUV.y = 1.0 - sliceAxis(uv.y * noteSize.y, noteSize.y, cornerY);

		if (isBlack)
			noteTexColor = texture(noteBlack, texUV);
		else
			noteTexColor = texture(noteWhite, texUV);

		gl_FragColor = vec4(noteTexColor.rgb * noteColor2, noteTexColor.a);
	}
	else
	{
		vec2 arrowUV = isRight ? vec2(1.0 - uv.x, uv.y) : uv;
		noteTexColor = texture(noteOOB, arrowUV);

		gl_FragColor = vec4(noteTexColor.rgb * noteColor, noteTexColor.a);
	}
})";
#pragma endregion
}

#pragma region Particles

float KeyHazeParticle::maxLife = 0.1;

KeyHazeParticle::KeyHazeParticle(uint8_t key, std::mt19937& random)
{
	std::uniform_real_distribution<double> dist(0.0, 1.0);

	this->key = key;
	life = maxLife;
	pos = { dist(random), dist(random) };
}

void KeyHazeParticle::Step(double delta)
{
	life -= delta;
	brightness = 1.0 - std::abs((life * 2 - maxLife) / maxLife);
}

float KeySparkParticle::maxLife = 0.2;
float KeySparkParticle::minSize = 0.7;

KeySparkParticle::KeySparkParticle(uint8_t key, std::mt19937& random)
{
	std::uniform_real_distribution<double> dist(0.0, 1.0);

	this->key = key;
	life = maxLife;
	pos = { dist(random), dist(random) };
	rotation = dist(random) * 2.0 - 1.0;
	flipped = dist(random) < 0.5;
}

void KeySparkParticle::Step(double delta)
{
	life -= delta;

	brightness = 1.0 - std::abs((life * 2 - maxLife) / maxLife);
	size = brightness * (1.0 - minSize) + minSize;
}

KeyDebrisParticle::KeyDebrisParticle(uint8_t key, std::mt19937& random)
{
	std::uniform_real_distribution<double> dist(0.0, 1.0);

	this->key = key;

	pos = { 0, 0 };
	vel = { (dist(random) - 0.5) * 0.01, dist(random) * 0.003 };
	life = 0.3;
	rotation = dist(random) * 2.0 - 1.0;
	size = dist(random) * 0.5 + 0.2;
}

void KeyDebrisParticle::Step(double delta)
{
	life -= delta;

	pos += glm::vec2(vel.x * delta * 60.0, vel.y * delta * 60.0);
	if (pos.y < 0) life = 0;
	vel.y -= 0.001 * delta * 60;
}

#pragma endregion 

inline uint32_t SampleNativeColor(uint16_t track, uint8_t channel, bool isBlack)
{
	size_t index = (((size_t)track << 4u) | (size_t)channel) % nativeSynthesiaColors.size();
	return isBlack ? nativeSynthesiaColorsDark[index] : nativeSynthesiaColors[index];
}

MIDIRendererSynthesia::MIDIRendererSynthesia(MIDIApp* app) : AbstractMIDIRenderer(app)
{ }

MIDIRendererSynthesia::~MIDIRendererSynthesia() = default;

void MIDIRendererSynthesia::Initialize()
{
	AbstractMIDIRenderer::Initialize();

	std::array<float, 8> quadVertices{
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f
	};

	std::array<int, 6> quadIndices{
		0, 1, 3,
		1, 2, 3
	};

#pragma region Rect shader initialization
	rectProgram = ShaderProgram::Create(rectVert, rectFrag);
	rectVAO = std::make_unique<VertexArray>();
	rectVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	rectIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	rectEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*rectVAO);

		rectVBO->Bind();
		rectVBO->SetData(quadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), (void*)0);

		rectEBO->Bind();
		rectEBO->SetData(quadIndices, GL_STATIC_DRAW);

		rectIBO->Bind();
		rectIBO->SetData(renderRects, GL_DYNAMIC_DRAW);

		rectVAO->SetFloatAttribute(1, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, position));
		rectVAO->SetFloatAttribute(2, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, size));
		rectVAO->SetFloatAttribute(3, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, rotation));
		rectVAO->SetFloatAttribute(4, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, uv0));
		rectVAO->SetFloatAttribute(5, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, uv1));
		rectVAO->SetIntAttribute(6, 1, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, textureIndex));
		rectVAO->SetIntAttribute(7, 1, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);
		glVertexAttribDivisor(7, 1);
	}

	// The sampler uniform only needs to be set once - it just points at a
	// texture unit, and we always bind the array to that same unit.
	{
		ShaderBind shaderBind(*rectProgram);
		rectProgram->SetInt("textures", TEXTURE_ARRAY_SLOT);
	}
#pragma endregion

#pragma region Note shader initialization
	notesProgram = ShaderProgram::Create(notesVert, notesFrag);
	notesVAO = std::make_unique<VertexArray>();
	notesVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*notesVAO);

		notesVBO->Bind();
		notesVBO->SetData(quadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), (void*)0);

		notesEBO->Bind();
		notesEBO->SetData(quadIndices, GL_STATIC_DRAW);

		notesIBO->Bind();
		notesIBO->SetData(renderNotes, GL_DYNAMIC_DRAW);

		notesVAO->SetFloatAttribute(1, 2, sizeof(RenderNoteSynthesia), offsetof(RenderNoteSynthesia, x));
		notesVAO->SetFloatAttribute(2, 2, sizeof(RenderNoteSynthesia), offsetof(RenderNoteSynthesia, width));
		notesVAO->SetIntAttribute(3, 1, sizeof(RenderNoteSynthesia), offsetof(RenderNoteSynthesia, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
	}

	{
		ShaderBind shaderBind(*notesProgram);
		notesProgram->SetInt("nativeColors", renderSettings.useNativeNoteColors ? 1 : 0);
	}
#pragma endregion

#pragma region Keyboard data initialization
	int b = 0;
	int w = 0;
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		if (IS_BLACK(i)) keyNum[i] = b++;
		else keyNum[i] = w++;
	}

	std::vector<uint8_t> blackIDs;
	blackIDs.reserve(53);
	std::vector<uint8_t> whiteIDs;
	whiteIDs.reserve(75);

	uint8_t keyID = 0;
	for (uint8_t key = 0; key < MIDI_KEYS; key++)
	{
		bool black = IS_BLACK(key);
		if (black) blackIDs.push_back(key);
		else whiteIDs.push_back(key);
	}

	int i = 0;
	for (auto& white : whiteIDs)
	{
		kbIDs[i++] = white;
	}
	for (auto& black : blackIDs)
	{
		kbIDs[i++] = black;
	}

	// InitializeTextures();
	UpdateStyle();
	InitializeParticleSystems();

	initialized = true;
#pragma endregion
}

void MIDIRendererSynthesia::CalculateKeyboardData()
{
	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast() + 1;

	if (renderSettings.style == SYNTHESIA_10 ||
		renderSettings.style == SYNTHESIA_9 ||
		renderSettings.style == GLOW)
	{
		initialKeyboardHeight = 0.158;
		initialBarHeight = 0.0045 * 1.5;
	}

	if (renderSettings.style == SYNTHESIA_8)
	{
		initialKeyboardHeight = 0.16;
		initialBarHeight = 0.007 * 1.5;
	}

	keyboardHeight = initialKeyboardHeight / (keyLast - keyFirst) * 128.0f;
	keyboardHeight = keyboardHeight / (1920.0 / 1080.0) * ((float)this->width / (float)this->height);
	if (keyboardHeight > 0.2468) keyboardHeight = 0.2468;

	if ((renderSettings.style == SYNTHESIA_9 || renderSettings.style == SYNTHESIA_10 || renderSettings.style == GLOW) && this->height > 0)
		keyboardHeight = std::min(keyboardHeight, 289.0f / (float)this->height);
}

void MIDIRendererSynthesia::LoadKeyboardTextures(SynthesiaStyle style)
{
	const std::filesystem::path keyboardPath = "./assets/textures/synthesia/keyboards";
	const std::filesystem::path particlePath = "./assets/textures/synthesia/particles";

	size_t numTextures = 0;
	switch (style)
	{
		case SYNTHESIA_10:
		case SYNTHESIA_9:
		{
			numTextures = S9_LAYER_COUNT;
			break;
		}
		case SYNTHESIA_8:
		{
			numTextures = S8_LAYER_COUNT;
			break;
		}
		case GLOW:
		{
			numTextures = GLOW_LAYER_COUNT;
			break;
		}
	}

	numTextures += PARTICLE_LAYER_COUNT;

	// layer -> file path, in the same order as the TextureLayer enum.
	std::vector<std::filesystem::path> layerPaths;
	switch (style)
	{
		case SYNTHESIA_10:
		case SYNTHESIA_9:
		{
			layerPaths = {
				keyboardPath / "s9/bar.png",
				keyboardPath / "s9/blackKeys.png",
				keyboardPath / "s9/blackKeysPressed.png",
				keyboardPath / "s9/whiteKeys.png",
				keyboardPath / "s9/whiteKeysPressed.png",
				keyboardPath / "s9/whiteKeyWhole.png",
				keyboardPath / "s9/whiteKeyWholePressed.png",
				keyboardPath / "s9/shadowLarge.png",
				keyboardPath / "s9/shadowUnpressed.png",
				keyboardPath / "s9/shadowPressed.png",
				particlePath / "keyDebris.png",
				particlePath / "keySpark.png",
				particlePath / "keyHaze.png"
			};
			break;
		}
		case SYNTHESIA_8:
		{
			layerPaths = {
				keyboardPath / "s8/bar.png",
				keyboardPath / "s7/blackKey.png",
				keyboardPath / "s7/blackKeyPressed.png",
				keyboardPath / "s8/whiteKeys.png",
				keyboardPath / "s8/whiteKeysPressed.png",
				keyboardPath / "s8/whiteKeyWhole.png",
				keyboardPath / "s8/whiteKeyWholePressed.png",
				particlePath / "keyDebris.png",
				particlePath / "keySpark.png",
				particlePath / "keyHaze.png"
			};
			break;
		}
		case GLOW:
		{
			layerPaths = {
				keyboardPath / "glow/bar.png",
				keyboardPath / "glow/blackKeys.png",
				keyboardPath / "glow/blackKeysPressed.png",
				keyboardPath / "glow/whiteKeys.png",
				keyboardPath / "glow/whiteKeysPressed.png",
				keyboardPath / "glow/whiteKeyWhole.png",
				keyboardPath / "glow/whiteKeyWholePressed.png",
				particlePath / "keyDebris.png",
				particlePath / "keySpark.png",
				particlePath / "keyHaze.png"
			};
			break;
		}
	}

	// GL_TEXTURE_2D_ARRAY requires every layer to share one width/height, so
	// probe every source image first and size the array to fit the largest.
	int maxWidth = 1;
	int maxHeight = 1;
	for (const auto& path : layerPaths)
	{
		int w, h;
		if (TextureArray::ProbeDimensions(Utils::TryGetStream(path), w, h))
		{
			maxWidth = std::max(maxWidth, w);
			maxHeight = std::max(maxHeight, h);
		}
	}

	textures = std::make_unique<TextureArray>(maxWidth, maxHeight, numTextures);

	for (int layer = 0; layer < numTextures; layer++)
	{
		layerUV[layer] = textures->LoadLayer(layer, Utils::TryGetStream(layerPaths[layer]));
	}
	textures->SetLinearFiltering();
}

void MIDIRendererSynthesia::InitializeParticleSystems()
{
	fullParticlesArray.clear();

	for (size_t i = 0; i < MIDI_KEYS; i++)
	{
		keyHazeParticles[i].clear();
		fullParticlesArray.push_back(&keyHazeParticles[i]);
		keySparkParticles[i].clear();
		fullParticlesArray.push_back(&keySparkParticles[i]);
	}

	fullParticlesArray.push_back(&keyDebrisParticles);
}

void MIDIRendererSynthesia::GenerateKeyLayoutArrays()
{
	if (!keyArrayDirty) return;

	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast() + 1;

	std::array<float, 5> additionalOffsets = { 0.0, 0.0, 0.0, 0.0, 0.0 };
	float blackKeyScale = 0.6f;

	if (renderSettings.style == SYNTHESIA_9 || renderSettings.style == SYNTHESIA_10 || renderSettings.style == GLOW)
	{
		additionalOffsets = { 0.02, -0.035, 0.0, -0.02, -0.03 };
		blackKeyScale = 0.63f;
	}
	
	const float blackKey2setOffset = 0.3f;
	const float blackKey3setOffset = 0.5f;

	float wdth;

	std::array<float, MIDI_KEYS> leftArray{};
	std::array<float, MIDI_KEYS> widthArray{};

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		if (!IS_BLACK(i))
		{
			leftArray[i] = keyNum[i];
			widthArray[i] = 1.0f;
			continue;
		}

		int _i = i + 1;
		wdth = blackKeyScale;
		int bknum = keyNum[i] % 5;
		float offset = wdth * 0.5f;
		if (bknum == 0) offset += wdth * 0.5f * blackKey2setOffset;
		if (bknum == 2) offset += wdth * 0.5f * blackKey3setOffset;
		if (bknum == 1) offset -= wdth * 0.5f * blackKey2setOffset;
		if (bknum == 4) offset -= wdth * 0.5f * blackKey3setOffset;

		offset -= additionalOffsets[bknum] * wdth * 0.5f;

		leftArray[i] = keyNum[_i] - offset;
		widthArray[i] = wdth;
	}

	float knmfn = leftArray[keyFirst];
	float knmln = leftArray[keyLast - 1] + widthArray[keyLast - 1];
	float width = knmln - knmfn;

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		keyPos[i] = (leftArray[i] - knmfn) / width;
		keyWidth[i] = widthArray[i] / width;
	}

	blackKeyWidth = blackKeyScale / width;
	whiteKeyWidth = 1.0f / width;
	keyArrayDirty = false;
}

void MIDIRendererSynthesia::LoadSequence(MIDISequence* sequence)
{
	if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
	AbstractMIDIRenderer::LoadSequence(sequence);

	colors.LoadColors();
	
	for (auto& id : startBlockIDs)
		id = 0;

	lastTime = 0;
}

void MIDIRendererSynthesia::UpdateRenderer()
{
	MIDIPlayerConfig* config = app->GetConfig();
	if (config->render.ConsumeKeyRangeChanged())
	{
		CalculateKeyboardData();
		keyArrayDirty = true;
	}

	GenerateKeyLayoutArrays();
}

void MIDIRendererSynthesia::UpdateStyle()
{
	GenerateKeyLayoutArrays();
	CalculateKeyboardData();

	std::filesystem::path noteWhitePath;
	std::filesystem::path noteBlackPath;

	float brightnessBoost = 1.0f;

	switch (renderSettings.style)
	{
		case SYNTHESIA_10:
		{
			noteWhitePath = "./assets/textures/synthesia/notes/s10/noteFull.png";
			noteBlackPath = noteWhitePath;
			break;
		}
		case SYNTHESIA_9:
		case SYNTHESIA_8:
		{
			noteWhitePath = "./assets/textures/synthesia/notes/s9/noteWhiteFull.png";
			noteBlackPath = "./assets/textures/synthesia/notes/s9/noteBlackFull.png";

			brightnessBoost = 2.0f;
			break;
		}
		case GLOW:
		{
			noteWhitePath = "./assets/textures/synthesia/notes/glow/noteFull.png";
			noteBlackPath = noteWhitePath;
			break;
		}
	}

	noteWhite = std::make_unique<GPUImage>(Utils::TryGetStream(noteWhitePath));
	noteBlack = std::make_unique<GPUImage>(Utils::TryGetStream(noteBlackPath));
	noteWhite->SetLinearFiltering();
	noteBlack->SetLinearFiltering();
	noteOOB = std::make_unique<GPUImage>(Utils::TryGetStream("./assets/textures/synthesia/notes/noteOOB.png"));

	{
		ShaderBind bind(*notesProgram);
		notesProgram->SetFloat("brightnessBoost", brightnessBoost);
		notesProgram->SetInt("noteWhite", 0);
		notesProgram->SetInt("noteBlack", 1);
		notesProgram->SetInt("noteOOB", 6);
	}

	LoadKeyboardTextures(renderSettings.style);
}

void MIDIRendererSynthesia::Render(double deltaTime)
{
	if (!initialized) return;
	rectDrawCount = 0;

	UpdateRenderer();

	sceneFramebuffer->Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// if background drawing
	if (renderSettings.renderBackground)
	{
		RenderBackground();
		RenderLines();

		RenderQuads(rectDrawCount);
		rectDrawCount = 0;
	}

	RenderNotes();
	if (renderSettings.showOutOfBoundNotes) RenderOutOfBoundNotes();

	if (renderSettings.renderKeySparkle)
	{
		GenerateParticles(deltaTime);
		RenderParticles(false);

		RenderQuads(rectDrawCount);
		rectDrawCount = 0;
	}

	RenderKeyboard();
	RenderQuads(rectDrawCount);
	rectDrawCount = 0;

	// haze glows over the keys, so it is drawn after the keyboard
	if (renderSettings.renderKeySparkle)
	{
		RenderParticles(true);
		UpdateParticles(deltaTime);

		RenderQuads(rectDrawCount);
		rectDrawCount = 0;
	}
	if (renderSettings.showKeyOctaves && renderSettings.style == SYNTHESIA_9) RenderOctaveTextOverlays();

	glDisable(GL_BLEND);

	sceneFramebuffer->Unbind();

	ResetKeyboardState();
}

void MIDIRendererSynthesia::RenderBackground()
{
	PushQuad(0.0, 0.0, 1.0, 1.0, 0xFF252525);
}

void MIDIRendererSynthesia::RenderLines()
{
	RenderMeasureLines();
	RenderDividerLines();
}

void MIDIRendererSynthesia::RenderMeasureLines()
{
	const float lineWidth = 0.02f;
	const float lineHeight = lineWidth * ((float)this->width / (float)this->height);

	if (!seq) return;
	std::vector<TimeSignatureEvent>& timeSigEvents = seq->timeSignatures;
	if (timeSigEvents.empty()) return;

	auto* renderView = app->GetRenderView();
	if (!renderView) return;

	TempoMap* tempoMap = seq->GetTempoMap();
	if (!tempoMap) return;

	const double playbackSeconds = app->GetTimer()->Elapsed();
	const long currentTicks = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);

	const double currentTime = isTimeBased ? playbackSeconds : (double)currentTicks;
	const double viewRegion = isTimeBased
		? (double)renderView->viewTicks / 1000.0
		: (double)renderView->viewTicks;

	const double invViewRegion = 1.0 / viewRegion;
	const double renderEnd = currentTime + viewRegion;

	long measureTime = 0;

	auto measurePos = [&](long ticks) -> double
		{
			return isTimeBased
				? tempoMap->TicksToSecsFromMap(seq->resolution, ticks)
				: (double)ticks;
		};

	for (size_t i = 0; i < timeSigEvents.size(); i++)
	{
		TimeSignatureEvent* timeSignature = &timeSigEvents[i];
		TimeSignatureEvent* futureTimeSignature =
			(i + 1 < timeSigEvents.size()) ? &timeSigEvents[i + 1] : nullptr;

		const long measureInc = static_cast<long>(seq->resolution * 4) / timeSignature->denominator;
		int beat = 0;

		long futureTimeSigTick = 0l;
		if (futureTimeSignature)
			futureTimeSigTick = isTimeBased
			? tempoMap->SecsToTicksFromMap(seq->resolution, (double)futureTimeSignature->tick / TIME_BASED_MULTIPLIER)
			: futureTimeSignature->tick;

		while (futureTimeSignature ? (measureTime < futureTimeSigTick) : (measurePos(measureTime) < renderEnd))
		{
			const double pos = measurePos(measureTime);

			const float linePos = (float)((pos - currentTime) * invViewRegion) * (1.0 - keyboardHeight) + keyboardHeight;
			const float top = linePos + whiteKeyWidth * lineHeight;
			const float bottom = linePos - whiteKeyWidth * lineHeight;

			if (beat == 0 && pos > currentTime && pos < renderEnd)
			{
				PushQuad(0, bottom, 1, top - bottom, 0xFF323232);
			}

			measureTime += measureInc;
			beat++;
			if (beat >= timeSignature->numerator) beat = 0;
		}
		if (futureTimeSignature) measureTime = futureTimeSigTick;
	}
}

void MIDIRendererSynthesia::RenderDividerLines()
{
	MIDIPlayerConfig* config = app->GetConfig();
	const int keyFirst = config->render.GetKeyFirst();
	const int keyLast = config->render.GetKeyLast() + 1;

	const float lineWidth = 0.02f;

	// divider lines
	for (int i = keyFirst; i <= keyLast; i++)
	{
		if (i >= MIDI_KEYS) continue;

		float line = keyPos[i];
		float left = line - whiteKeyWidth * lineWidth;
		float right = line + whiteKeyWidth * lineWidth;

		if (i % 12 == 0)
		{
			PushQuad(left, 0.0f, right - left, 1.0f, 0xFF505050);
		}
		if (i % 12 == 5)
		{
			PushQuad(left, 0.0f, right - left, 1.0f, 0xFF323232);
		}
	}
}

void MIDIRendererSynthesia::RenderKeyboard()
{
	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast();

	if (IS_BLACK(keyFirst)) keyFirst--;
	if (IS_BLACK(keyLast)) keyLast++;

	const bool s9Bar = renderSettings.style == SYNTHESIA_9 || renderSettings.style == SYNTHESIA_10;
	
	// S9/S10: the bar is a fixed 10px, independent of the screen size
	const float pxScale = keyboardHeight * (float)height / 289.0f;
	const float barPx = 10.0f * pxScale / (float)height;
	const float BLACK_KEY_OVERFLOW_PX = 3.0f * pxScale;
	const float BLACK_KEY_OVERFLOW_PRESSED_PX = 1.2f * pxScale;
	
	const float barH = initialBarHeight * keyboardHeight / 0.2468f;
	float keyTop = s9Bar ? keyboardHeight - barPx : keyboardHeight * (1.0f - initialBarHeight);

#pragma region Synthesia 9/10 Keyboard
	if (renderSettings.style == SYNTHESIA_9 || renderSettings.style == SYNTHESIA_10)
	{
		const float frac = 1.0f / 7.0f;
		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (i >= MIDI_KEYS) continue;

			if (IS_BLACK(i)) continue;
			float creamR = 1.0f;
			float creamG = 0.98f;
			float creamB = 0.9f;

			uint32_t creamColor = Utils::PackRGBA(creamR, creamG, creamB, 1.0f, Utils::ARGB);

			size_t part = keyNum[i] % 7;

			float split = 0.5f;
			if (part == 3) split = 0.3f;
			if (part == 6) split = 0.7f;

			float uvLeft = part * frac;
			float uvRight = (part + 1) * frac;
			float uvMiddle = (part + split) * frac;

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];
			float middle = left + (right - left) * split;

			uint32_t color = 0x7F000000 | keyStates[i].color;

			if (i == keyLast)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, S9_LAYER_KEY_WHITE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, S9_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, S9_LAYER_KEY_WHITE, creamColor);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, S9_LAYER_KEY_WHITE_WHOLE, creamColor);
				}
			}
			else if (i == keyFirst)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, S9_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, S9_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, S9_LAYER_KEY_WHITE_WHOLE, creamColor);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, S9_LAYER_KEY_WHITE, creamColor);
				}
			}
			else
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, S9_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, S9_LAYER_KEY_WHITE, creamColor);
				}
			}

#pragma region Shadows
			float aspectRatio = (float)this->width / (float)this->height;
			if (i != 0 && i != keyFirst)
			{
				const auto& RenderLarge = [this, left, keyTop, aspectRatio]()
					{
						float shadowRight = left + keyTop * textures->GetLayerAspectRatio(S9_LAYER_SHADOW_LARGE) / aspectRatio;
						PushQuad(left, 0, shadowRight - left, keyTop, 0, 0, 1, 1, S9_LAYER_SHADOW_LARGE);
					};

				const auto& RenderUnpressed = [this, left, keyTop, aspectRatio, i]()
					{
						float keyRight = keyPos[i - 1] + keyWidth[i - 1];
						float shadowRight = keyRight + keyTop * textures->GetLayerAspectRatio(S9_LAYER_SHADOW_UNPRESSED) / aspectRatio;
						PushQuad(keyRight, 0, shadowRight - keyRight, keyTop, 0, 0, 1, 1, S9_LAYER_SHADOW_UNPRESSED);
					};

				const auto& RenderPressed = [this, left, keyTop, aspectRatio, i]()
					{
						float keyRight = keyPos[i - 1] + keyWidth[i - 1];
						float shadowRight = keyRight + keyTop * textures->GetLayerAspectRatio(S9_LAYER_SHADOW_PRESSED) / aspectRatio;
						PushQuad(keyRight, 0, shadowRight - keyRight, keyTop, 0, 0, 1, 1, S9_LAYER_SHADOW_PRESSED);
					};

				if (keyStates[i].pressed)
				{
					if (IS_BLACK(i - 1))
					{
						if (!keyStates[i - 2].pressed) RenderLarge();
						if (keyStates[i - 1].pressed) RenderPressed();
						else RenderUnpressed();
					}
					else
					{
						if (!keyStates[i - 1].pressed) RenderLarge();
					}
				}
				else
				{
					if (IS_BLACK(i - 1))
					{
						if (!keyStates[i - 1].pressed) RenderPressed();
					}
				}
			}

#pragma endregion
		}
		// split in two to allow colored bar
		const float barFrac = 3.0f / 11.0f;
		const float barSplitHeight = BLACK_KEY_OVERFLOW_PX / (float)height;
		const float barTopHeight = barPx - barSplitHeight;

		ImVec4 barColor = config->render.GetBarColor();
		uint32_t barColorPacked = Utils::PackRGBA(barColor.x, barColor.y, barColor.z, 1.0, Utils::ARGB);
		PushQuad(0, keyTop, 1, barSplitHeight, 0, 0, 1, barFrac, S9_LAYER_BAR, barColorPacked);
		PushQuad(0, keyTop + barSplitHeight, 1, barTopHeight, 0, barFrac, 1, 1.0f - barFrac, S9_LAYER_BAR);

		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (!IS_BLACK(i)) continue;

			size_t part = keyNum[i] % 5;

			float bKeyBottom = keyboardHeight / 2.87f;
			// fixed px overflow past the white key top
			float bKeyTopPressed = keyTop + BLACK_KEY_OVERFLOW_PRESSED_PX / (float)height;
			float bKeyTopUnpressed = keyTop + BLACK_KEY_OVERFLOW_PX / (float)height;

			float uvLeft = part * (1.0f / 5.0f);
			float uvRight = (part + 1) * (1.0f / 5.0f);

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];

			if (keyStates[i].pressed)
				PushQuad(left, bKeyBottom, right - left, bKeyTopPressed - bKeyBottom, uvLeft, 0, uvRight, 1, S9_LAYER_KEY_BLACK_PRESSED, 0x7F000000 | keyStates[i].color);
			else
				PushQuad(left, bKeyBottom, right - left, bKeyTopUnpressed - bKeyBottom, uvLeft, 0, uvRight, 1, S9_LAYER_KEY_BLACK);
		}
	}
	
#pragma endregion

#pragma region Synthesia 8 Keyboard
	if (renderSettings.style == SYNTHESIA_8)
	{
		const float frac = 1.0f / 7.0f;
		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (i >= MIDI_KEYS) continue;

			if (IS_BLACK(i)) continue;
			float creamR = 1.0f;
			float creamG = 0.98f;
			float creamB = 0.9f;

			uint32_t creamColor = Utils::PackRGBA(creamR, creamG, creamB, 1.0f, Utils::ARGB);

			size_t part = keyNum[i] % 7;

			float split = 0.5f;
			if (part == 3) split = 0.3f;
			if (part == 6) split = 0.7f;

			float uvLeft = part * frac;
			float uvRight = (part + 1) * frac;
			float uvMiddle = (part + split) * frac;

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];
			float middle = left + (right - left) * split;

			uint32_t color = 0x7F000000 | keyStates[i].color;

			if (i == keyLast)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, S8_LAYER_KEY_WHITE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, S8_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, S8_LAYER_KEY_WHITE, 0x7FFFFFFF);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, S8_LAYER_KEY_WHITE_WHOLE, 0x7FFFFFFF);
				}
			}
			else if (i == keyFirst)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, S8_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, S8_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, S8_LAYER_KEY_WHITE_WHOLE, 0x7FFFFFFF);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, S8_LAYER_KEY_WHITE, 0x7FFFFFFF);
				}
			}
			else
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, S8_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, S8_LAYER_KEY_WHITE, 0x7FFFFFFF);
				}
			}
		}

		const float barFrac = 1.0f / 3.0f;
		const float barSplitHeight = barFrac * barH;

		ImVec4 barColor = config->render.GetBarColor();
		uint32_t barColorPacked = Utils::PackRGBA(barColor.x, barColor.y, barColor.z, 1.0, Utils::ARGB);
		PushQuad(0, keyTop, 1, barSplitHeight, 0, 0, 1, barFrac, S8_LAYER_BAR, barColorPacked);
		PushQuad(0, keyTop + barSplitHeight, 1, barH - barSplitHeight, 0, barFrac, 1, 1.0f - barFrac, S8_LAYER_BAR);

		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (!IS_BLACK(i)) continue;

			float bKeyBottom = keyboardHeight / 2.65f;
			float bKeyTop = keyTop + barSplitHeight;

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];

			if (keyStates[i].pressed)
				PushQuad(left, bKeyBottom, right - left, bKeyTop - bKeyBottom, 0, 0, 1, 1, S8_LAYER_KEY_BLACK_PRESSED, 0x7F000000 | keyStates[i].color);
			else
				PushQuad(left, bKeyBottom, right - left, bKeyTop - bKeyBottom, 0, 0, 1, 1, S8_LAYER_KEY_BLACK);
		}
	}
#pragma endregion

#pragma region Glow keyboard
	if (renderSettings.style == GLOW)
	{
		const float frac = 1.0f / 7.0f;
		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (i >= MIDI_KEYS) continue;

			if (IS_BLACK(i)) continue;
			float creamR = 1.0f;
			float creamG = 0.98f;
			float creamB = 0.9f;

			uint32_t creamColor = Utils::PackRGBA(creamR, creamG, creamB, 1.0f, Utils::ARGB);

			size_t part = keyNum[i] % 7;

			float split = 0.5f;
			if (part == 3) split = 0.3f;
			if (part == 6) split = 0.7f;

			float uvLeft = part * frac;
			float uvRight = (part + 1) * frac;
			float uvMiddle = (part + split) * frac;

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];
			float middle = left + (right - left) * split;

			uint32_t color = 0x7F000000 | keyStates[i].color;

			if (i == keyLast)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, GLOW_LAYER_KEY_WHITE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, GLOW_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, GLOW_LAYER_KEY_WHITE, 0x7FFFFFFF);
					PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, GLOW_LAYER_KEY_WHITE_WHOLE, 0x7FFFFFFF);
				}
			}
			else if (i == keyFirst)
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, GLOW_LAYER_KEY_WHITE_WHOLE_PRESSED, color);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, GLOW_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, GLOW_LAYER_KEY_WHITE_WHOLE, 0x7FFFFFFF);
					PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, GLOW_LAYER_KEY_WHITE, 0x7FFFFFFF);
				}
			}
			else
			{
				if (keyStates[i].pressed)
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, GLOW_LAYER_KEY_WHITE_PRESSED, color);
				}
				else
				{
					PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, GLOW_LAYER_KEY_WHITE, 0x7FFFFFFF);
				}
			}
		}

		PushQuad(0, keyTop, 1, barH, 0, 0, 1, 1, GLOW_LAYER_BAR);
		for (int i = keyFirst; i <= keyLast; i++)
		{
			if (!IS_BLACK(i)) continue;

			size_t part = keyNum[i] % 5;

			float bKeyBottom = keyboardHeight / 2.8f;
			float bKeyTopPressed = keyTop + keyboardHeight * 0.006;
			float bKeyTopUnpressed = keyTop + keyboardHeight * 0.015;

			float uvLeft = part * (1.0f / 5.0f);
			float uvRight = (part + 1) * (1.0f / 5.0f);

			float left = keyPos[i];
			float right = keyPos[i] + keyWidth[i];

			if (keyStates[i].pressed)
				PushQuad(left, bKeyBottom, right - left, bKeyTopPressed - bKeyBottom, uvLeft, 0, uvRight, 1, GLOW_LAYER_KEY_BLACK_PRESSED, 0x7F000000 | keyStates[i].color);
			else
				PushQuad(left, bKeyBottom, right - left, bKeyTopUnpressed - bKeyBottom, uvLeft, 0, uvRight, 1, GLOW_LAYER_KEY_BLACK);
		}
	}
#pragma endregion
}

void MIDIRendererSynthesia::RenderQuads(size_t count)
{
	if (count == 0) return;

	ShaderBind shaderBind(*rectProgram);
	VertexArrayBind vaoBind(*rectVAO);
	BufferBind iboBind(*rectIBO);
	TextureArrayBind texBind(*textures, TEXTURE_ARRAY_SLOT);

	rectProgram->SetFloat("aspect", (float)width / (float)height);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(TexturedRectInstance) * count, renderRects.data());
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, count);
}

void MIDIRendererSynthesia::RenderNotes()
{
	NoteFrame f;
	if (!BeginNoteFrame(f)) return;

	ShaderBind shaderBind(*notesProgram);

	TextureBind noteWhiteBind(*noteWhite, 0);
	TextureBind noteBlackBind(*noteBlack, 1);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);
	if (notesIBO) notesIBO->Bind();

	const double noteAreaPx = (double)height * (1.0 - keyboardHeight);

	notesProgram->SetFloat("kbHeight", keyboardHeight);
	notesProgram->SetVec2("texSizeWhite", glm::vec2(noteWhite->width, noteWhite->height));
	notesProgram->SetVec2("texSizeBlack", glm::vec2(noteBlack->width, noteBlack->height));
	notesProgram->SetVec2("noteToPx", glm::vec2((float)width, (float)noteAreaPx));

	// Tallest min height any note gets (Synthesia: crown + heel + 1px, scaled by note width).
	auto minNotePx = [&](const GPUImage& tex, float keyW)
	{
		const float s = (keyW * width + 2.0f) / tex.width;
		return tex.height * s + 1.0f;
	};
	const double minNote = std::max(minNotePx(*noteWhite, keyWidth[0]), minNotePx(*noteBlack, keyWidth[1]));

	CollectNotes(f,
		{ .noteAreaPx = noteAreaPx, .yBase = noteAreaPx,
		  .minVisualPx = minNote * 1.001 }, // slightly high: over-collecting is safe
		[&](uint8_t id, uint16_t track, uint8_t channel)
		{
			keyStates[id].pressed = true;
			keyStates[id].color = renderSettings.useNativeNoteColors
				? SampleNativeColor(track, channel, blackArr[id])
				: colors.GetColor(track, channel);
		});

	size_t noteID = 0, batchCount = 0;
	ForEachNote(f, [&](const NoteRef& n)
		{
			renderNotes[noteID++] = RenderNoteSynthesia(
				keyPos[n.id], (float)n.yStart,
				keyWidth[n.id], (float)(n.yEnd - n.yStart),
				((renderSettings.useNativeNoteColors ?
					SampleNativeColor(n.track, n.channel, n.isBlack) :
					colors.GetColor(n.track, n.channel)) & 0xFFFFFF) | (n.isBlack << 24)
			);

			if (noteID >= NOTE_BUFFER_SIZE)
			{
				FlushNotes(NOTE_BUFFER_SIZE);
				if (++batchCount >= NOTES_MAX_BATCHES) { glFlush(); batchCount = 0; }
				noteID = 0;
			}
		});
	if (noteID != 0) FlushNotes(noteID);

	EndNoteFrame(f);

	lastTime = f.tick;
}

void MIDIRendererSynthesia::RenderOutOfBoundNotes()
{
	if (!seq) return;
	auto& notes = seq->mergedNotes;
	if (notes.empty()) return;
	auto* renderView = app->GetRenderView();
	if (!renderView) return;

	double playbackSeconds = app->GetTimer()->Elapsed();
	TempoMap* tempoMap = seq->GetTempoMap();
	long time = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000 : (double)renderView->viewTicks;
	const double invViewRegion = 1.0 / viewRegion;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast();

	size_t nID = 0;
	size_t batchCount = 0;

	ShaderBind shaderBind(*notesProgram);
	TextureBind tex(*noteOOB, 6);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);
	if (notesIBO) notesIBO->Bind();

	for (uint8_t id : kbIDs)
	{
		if (id >= keyFirst && id <= keyLast) continue;

		uint16_t lastTrack = 0;
		uint8_t lastChannel = 0;
		uint32_t lastTick = 0;
		uint32_t lastGate = 0;
		uint32_t lastNote = 0;

		NoteSequence& notesNote = notes[id];

		size_t noteBegin = startBlockIDs[id] * NOTE_BLOCK_SIZE;

		for (size_t i = noteBegin; i < notesNote.Size(); ++i)
		{
			uint32_t nTick = notesNote.tick[i];
			uint32_t nGate = notesNote.gate[i];
			uint8_t nNote = notesNote.GetKey(i);
			uint16_t nTrack = notesNote.GetTrack(i);
			uint8_t nChannel = notesNote.GetChannel(i);

			double noteStart = isTimeBased ? (double)nTick * invTimeMultiplier : (double)nTick;
			
			if (noteStart < accTime) continue;
			if (noteStart > accTime + viewRegion) break;

			if (lastNote &&
				nTick == lastTick &&
				nNote == lastNote &&
				nChannel == lastChannel &&
				nTrack == lastTrack &&
				nGate == lastGate)
			{
				continue;
			}

			lastTick = nTick;
			lastNote = nNote;
			lastChannel = nChannel;
			lastTrack = nTrack;
			lastGate = nGate;

			float arrowX = 0.0f;

			bool isLeft = nNote < keyFirst;
			if (isLeft) arrowX = 0.015;
			else arrowX = 0.985;

			float arrowY = (float)((noteStart - accTime) * invViewRegion);

			float arrowWidth = 0.023;
			float arrowHeight = arrowWidth * ((float)width / (float)height);

			arrowX -= arrowWidth * 0.5f;
			arrowY -= arrowHeight * 0.5f;

			renderNotes[nID++] = {
				arrowX, arrowY,
				arrowWidth, arrowHeight,
				((renderSettings.useNativeNoteColors ?
				SampleNativeColor(nTrack, nChannel, false) :
					colors.GetColor(nTrack, nChannel)) & 0xFFFFFF) | (1 << 25) | ((!isLeft) << 26)
			};

			if (nID >= NOTE_BUFFER_SIZE)
			{
				FlushNotes(NOTE_BUFFER_SIZE);
				batchCount++;
				if (batchCount >= NOTES_MAX_BATCHES)
				{
					glFlush();
					batchCount = 0;
				}
				nID = 0;
			}
		}
	}

	if (nID != 0)
	{
		FlushNotes(nID);
		batchCount++;
		if (batchCount >= NOTES_MAX_BATCHES)
		{
			glFlush();
		}
	}
}

void MIDIRendererSynthesia::FlushNotes(size_t count)
{
	if (count == 0) return;
	
	if (!notesIBO) return;

	GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
	void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, count * sizeof(RenderNoteSynthesia), mapFlags);
	if (ptr)
	{
		memcpy(ptr, renderNotes.data(), count * sizeof(RenderNoteSynthesia));
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}

	glDrawElementsInstanced(
		GL_TRIANGLES,
		6,
		GL_UNSIGNED_INT,
		nullptr,
		count
	);
}

void MIDIRendererSynthesia::GenerateParticles(double deltaTime)
{
	std::uniform_real_distribution<double> gen(0.0, 1.0);

	double framesElapsed = deltaTime * 60.0;
	double hazeChance = 1.0 - std::pow(1.0 - 0.5, framesElapsed);
	double sparkChance = 1.0 - std::pow(1.0 - 0.5, framesElapsed);
	double debrisChance = 1.0 - std::pow(1.0 - 0.02, framesElapsed);

	for (size_t i = 0; i < MIDI_KEYS; i++)
	{
		if (!keyStates[i].pressed)
		{
			if (keyHazeParticles[i].size() > 0) keyHazeParticles[i].clear();
			if (keySparkParticles[i].size() > 0) keySparkParticles[i].clear();
			continue;
		}

		if (gen(random) < hazeChance && keyHazeParticles[i].size() < 20) keyHazeParticles[i].push_back(std::make_unique<KeyHazeParticle>(i, random));
		if (gen(random) < sparkChance && keySparkParticles[i].size() < 20) keySparkParticles[i].push_back(std::make_unique<KeySparkParticle>(i, random));
		if (gen(random) < debrisChance && keyDebrisParticles.size() < 200) keyDebrisParticles.push_back(std::make_unique<KeyDebrisParticle>(i, random));
	}
}

void MIDIRendererSynthesia::RenderParticles(bool haze)
{
	int particleTexOffset = 0;
	if (renderSettings.style == SYNTHESIA_9 || renderSettings.style == SYNTHESIA_10)
		particleTexOffset = S9_LAYER_COUNT;
	if (renderSettings.style == SYNTHESIA_8)
		particleTexOffset = S8_LAYER_COUNT;
	if (renderSettings.style == GLOW)
		particleTexOffset = GLOW_LAYER_COUNT;

	float aspect = (float)width / (float)height;
	if (!haze)
	for (auto& particle : keyDebrisParticles)
	{
		KeyDebrisParticle* p = dynamic_cast<KeyDebrisParticle*>(particle.get());
		float left = keyPos[p->key];
		float right = keyPos[p->key] + keyWidth[p->key];
		glm::vec2 pos = { (left + right) / 2.0, keyboardHeight };

		glm::vec2 offset = p->pos;
		offset.y *= aspect;
		offset *= keyboardHeight / 0.15;
		pos += offset;
		float sizeX = whiteKeyWidth * p->size;
		float sizeY = sizeX * aspect;

		PushRotatedQuad(pos.x - sizeX * 0.5f, pos.y - sizeY * 0.5f, sizeX, sizeY, p->rotation, 0, 0, 1, 1, particleTexOffset+LAYER_PARTICLE_DEBRIS);
	}

	if (!haze)
	for (auto& particleArr : keySparkParticles)
	{
		for (auto& particle : particleArr)
		{
			KeySparkParticle* p = dynamic_cast<KeySparkParticle*>(particle.get());

			float left = keyPos[p->key];
			float right = keyPos[p->key] + keyWidth[p->key];
			float width = right - left;
			left += width * 0.4f;
			right -= width * 0.4f;
			glm::vec2 pos = { left + (right - left) * p->pos.x, keyboardHeight + (right - left) * p->pos.y * aspect };

			uint32_t blendCol = keyStates[p->key].color;
			blendCol = Utils::BlendRGBA(blendCol, Utils::PackRGBA(1.0, 1.0, 1.0, 1.0), 0.8);

			float ang = p->rotation / 5.0f;
			float sizeX = width * p->size * 1.6f;
			float sizeY = sizeX * aspect;

			if (p->flipped)
			{
				PushRotatedQuad(pos.x - sizeX * 0.5, pos.y - sizeY * 0.5, sizeX, sizeY, ang, 0, 0, 1, 1, particleTexOffset+LAYER_PARTICLE_SPARKLE, blendCol);
			}
			else
			{
				PushRotatedQuad(pos.x - sizeX * 0.5, pos.y - sizeY * 0.5, sizeX, sizeY, ang, 1, 0, 0, 1, particleTexOffset+LAYER_PARTICLE_SPARKLE, blendCol);
			}
		}
	}

	if (haze)
	for (auto& particleArr : keyHazeParticles)
	{
		for (auto& particle : particleArr)
		{
			KeyHazeParticle* p = dynamic_cast<KeyHazeParticle*>(particle.get());

			float left = keyPos[p->key];
			float right = keyPos[p->key] + keyWidth[p->key];
			float width = right - left;
			left += width * 0.4f;
			right -= width * 0.4f;
			glm::vec2 pos = { left + (right - left) * p->pos.x, keyboardHeight + (right - left) * p->pos.y * aspect };
			pos.y += keyboardHeight * 0.02f;

			float sizeX = width * 1.4f;
			float sizeY = sizeX * aspect;

			uint32_t alpha = (uint32_t)(std::clamp(p->brightness, 0.0f, 1.0f) * 0.2f * 127.0f) << 24;
			PushQuad(pos.x - sizeX, pos.y - sizeY, sizeX * 2.0f, sizeY * 2.0f, 0, 0, 1, 1, particleTexOffset+LAYER_PARTICLE_HAZE, alpha | 0xFFFFFF);
		}
	}
}

void MIDIRendererSynthesia::UpdateParticles(double deltaTime)
{
	for (auto& particleArray : fullParticlesArray)
	{
		for (size_t i = 0; i < particleArray->size();)
        {
            Particle& p = *(*particleArray)[i];

            p.Step(deltaTime);

            if (p.life <= 0.0f)
            {
				if (i != particleArray->size() - 1)
				{
					std::swap((*particleArray)[i], particleArray->back());
				}
                particleArray->pop_back();
                continue;
            }

            ++i;
        }
	}
}

void MIDIRendererSynthesia::RenderOctaveTextOverlays()
{
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();

	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast() + 1;


	for (size_t k = keyFirst; k < keyLast; k++)
	{
		if (k % 12 != 0) continue;

		int i = (k / 12) - 1;

		float left = keyPos[k];
		float right = keyPos[k] + keyWidth[k];

		float size = 1.2f;
		float spacing = 0.6;
		float offset = 0.0f;
		float textHeight = size;

		if (i >= 10)
		{
			textHeight = 0.75f * size;
			spacing = 0.55f;
			offset = -0.02f;
		}

		if (i == -1)
		{
			textHeight = 0.8f * size;
			spacing = 0.55f;
			offset = -0.02f;
		}

		textHeight *= right - left;

		std::string txt = "C" + std::to_string(i);

		float middle =
			(right + left) * 0.5f +
			(right - left) * offset;

		float x = middle * width;

		// scale with key width only (0.5625 keeps the old 1080p look), capped at the 1080p size
		float fontSize = textHeight * 0.5625f * std::min((float)width, 1920.0f);
		float offsetY = (right - left) * width * 0.18f;
		float y = height - offsetY - fontSize;

		ImVec2 textSize = ImGui::CalcTextSize(txt.c_str());
		float scale = fontSize / ImGui::GetFontSize();

		ImVec2 pos(
			x - textSize.x * scale * 0.5f,
			y
		);

		ImU32 col = IM_COL32(0, 0, 0, 92);

		if (k == 5 * 12)
			col = IM_COL32(0, 0, 0, 163);

		drawList->AddText(
			ImGui::GetFont(),
			fontSize,
			pos,
			col,
			txt.c_str()
		);
	}
}

void MIDIRendererSynthesia::RenderSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();

	if (ImGui::BeginTabBar("##renderSettings"))
	{
		if (ImGui::BeginTabItem("General"))
		{
			{
				ForumLayout layout("##sGeneral");

				layout.AddField("##synthesiaStyle", "Synthesia style",
					[&]()
					{
						auto style = renderSettings.style;
						std::string previewText = "";

						switch (style)
						{
						case SYNTHESIA_8:
							previewText = "Synthesia 8";
							break;
						case SYNTHESIA_9:
							previewText = "Synthesia 9";
							break;
						case SYNTHESIA_10:
							previewText = "Synthesia 10";
							break;
						case GLOW:
							previewText = "Glow";
							break;
						}

						if (ImGui::BeginCombo("", previewText.c_str()))
						{
							if (ImGui::Selectable("Synthesia 8", style == SYNTHESIA_8))
							{
								renderSettings.style = SYNTHESIA_8;
								UpdateStyle();
							}
							if (ImGui::Selectable("Synthesia 9", style == SYNTHESIA_9))
							{
								renderSettings.style = SYNTHESIA_9;
								UpdateStyle();
							}
							if (ImGui::Selectable("Synthesia 10", style == SYNTHESIA_10))
							{
								renderSettings.style = SYNTHESIA_10;
								UpdateStyle();
							}
							if (ImGui::Selectable("Glow", style == GLOW))
							{
								renderSettings.style = GLOW;
								UpdateStyle();
							}

							ImGui::EndCombo();
						}
					});

				layout.AddFieldWithTooltip("##nativeCols", "Native note colors",
					"Uses built-in synthesia colors instead of the color palette shared between renderers.",
					[&]()
					{
						if (ImGui::Checkbox("", &renderSettings.useNativeNoteColors))
						{
							ShaderBind shaderBind(*notesProgram);
							notesProgram->SetInt("nativeColors", renderSettings.useNativeNoteColors ? 1 : 0);
						}
					});

				layout.AddField("##oobNotes", "Out-of-bound notes",
					[&]()
					{
						ImGui::Checkbox("", &renderSettings.showOutOfBoundNotes);
					});

				layout.AddField("##background", "Background",
					[&]()
					{
						ImGui::Checkbox("", &renderSettings.renderBackground);
					});
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Keyboard"))
		{
			{
				ForumLayout layout("##sKeyboard");

				layout.AddField("##keySpark", "Key sparkles",
					[&]()
					{
						ImGui::Checkbox("", &renderSettings.renderKeySparkle);
					});

				layout.AddField("##barColor", "Bar color",
					[&]()
					{
						ImVec4 barColor = config->render.GetBarColor();
						if (ImGui::ColorEdit3("", &barColor.x))
						{
							config->render.SetBarColor(barColor.x, barColor.y, barColor.z);
						}
					});
			}

			ImGui::EndTabItem();
		}

		RenderStyleSettings();

		ImGui::EndTabBar();
	}

	AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererSynthesia::RenderStyleSettings()
{
	switch (renderSettings.style)
	{
	case SYNTHESIA_9:
	{
		if (ImGui::BeginTabItem("Synthesia 9"))
		{
			{
				ForumLayout layout("##sStyle");

				layout.AddField("##keyOctaves", "Key octaves",
					[&]()
					{
						ImGui::Checkbox("", &renderSettings.showKeyOctaves);
					});
			}

			ImGui::EndTabItem();
		}
		break;
	}
	case SYNTHESIA_10:
		break;
	default:
		break;
	}

}

void MIDIRendererSynthesia::ResetSettings()
{
	renderSettings = SynthesiaRenderSettings::Default();
}

void MIDIRendererSynthesia::OnResize(int width, int height)
{
	AbstractMIDIRenderer::OnResize(width, height);
	this->width = width;
	this->height = height;
	CalculateKeyboardData();
}

YAML::Node MIDIRendererSynthesia::GetSettings()
{
	YAML::Node node;
	node["style"] = static_cast<int>(renderSettings.style);
	node["nativeColors"] = renderSettings.useNativeNoteColors;
	node["oobNotes"] = renderSettings.showOutOfBoundNotes;
	node["keySparkle"] = renderSettings.renderKeySparkle;
	node["background"] = renderSettings.renderBackground;
	node["keyOctaves"] = renderSettings.showKeyOctaves;

	return node;
}

void MIDIRendererSynthesia::LoadSettings(const YAML::Node& node)
{
	if (!node) return;

	int styleInt;
	LoadValue(node, "style", styleInt);
	renderSettings.style = static_cast<SynthesiaStyle>(styleInt);

	LoadValue(node, "nativeColors", renderSettings.useNativeNoteColors);
	LoadValue(node, "oobNotes", renderSettings.showOutOfBoundNotes);
	LoadValue(node, "keySparkle", renderSettings.renderKeySparkle);
	LoadValue(node, "background", renderSettings.renderBackground);
	LoadValue(node, "keyOctaves", renderSettings.showKeyOctaves);

	UpdateStyle();
}

void MIDIRendererSynthesia::ResetKeyboardState()
{
	for (auto& k : keyStates)
	{
		k.pressed = false;
		k.color = 0;
	}
}

void MIDIRendererSynthesia::KillAllParticles()
{
	InitializeParticleSystems();
}

void MIDIRendererSynthesia::ResetRenderer()
{
	KillAllParticles();
	ResetKeyboardState();
}