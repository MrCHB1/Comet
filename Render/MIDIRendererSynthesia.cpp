#include "MIDIRendererSynthesia.h"
#include "Utils.h"
#include "App/MIDIApp.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"
#include "RenderView.h"
#include "App/Dialog/DialogMacros.h"
#include <memory>
#include <algorithm>

#define LOAD_VAL(n, key, target) \
        if (n && n[key]) { target = n[key].as<std::decay_t<decltype(target)>>(); }

#pragma region Shaders
inline constexpr const char* rectVert = R"(#version 330 core
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

inline constexpr const char* rectFrag = R"(#version 330 core
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

inline constexpr const char* notesVert = R"(#version 330 core
layout (location = 0) in vec2 vPos;
layout (location = 1) in vec2 aPos;
layout (location = 2) in vec2 aSize;
layout (location = 3) in uint aMeta;

out vec2 noteSize;
out vec2 uv;
flat out uint meta;

uniform float kbHeight;
uniform float capHeight;

void main()
{
	bool isOOB   = (aMeta & (1u << 25u)) != 0u;

	uv = vPos;
	noteSize = aSize;
	if (noteSize.y < capHeight * 2.0 && !isOOB) noteSize.y = capHeight * 2.0;
	meta = aMeta;

	vec2 pos = aPos + vPos * noteSize;
	pos.y = pos.y * (1.0 - kbHeight) + kbHeight;
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
})";

inline constexpr const char* notesFrag = R"(#version 330 core
in vec2 noteSize;
in vec2 uv;
flat in uint meta;

uniform float capAspectRatio;
uniform float aspectRatio;
uniform float capHeight;

uniform sampler2D noteWhiteBody;
uniform sampler2D noteWhiteTop;
uniform sampler2D noteWhiteBottom;
uniform sampler2D noteBlackBody;
uniform sampler2D noteBlackTop;
uniform sampler2D noteBlackBottom;
uniform sampler2D noteOOB;

uniform bool nativeColors;

uniform float brightnessBoost;

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
		if (isBlack)
			noteTexColor = texture(noteBlackBody, uv);
		else
			noteTexColor = texture(noteWhiteBody, uv);

		float bottomDistance = uv.y * noteSize.y;

		if (bottomDistance < capHeight)
		{
			vec2 capUV;
			capUV.x = uv.x;
			capUV.y = bottomDistance / capHeight;

			if (isBlack)
				noteTexColor = texture(noteBlackBottom, capUV);
			else
				noteTexColor = texture(noteWhiteBottom, capUV);
		}

		float topDistance = (1.0 - uv.y) * noteSize.y;

		if (topDistance < capHeight)
		{
			vec2 capUV;
			capUV.x = uv.x;
			capUV.y = 1.0 - topDistance / capHeight;

			if (isBlack)
				noteTexColor = texture(noteBlackTop, capUV);
			else
				noteTexColor = texture(noteWhiteTop, capUV);
		}

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

#pragma region Particles

float KeyHazeParticle::maxLife = 0.2;

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

inline std::vector<uint32_t> nativeSynthesiaColors = {
	// blue
	0x80A3CB,
	0x2A5EA7,
	// green
	0x9DE04D,
	0x4C9301,
	// orange
	0xF8A829,
	0xEF7100,
	// yellow
	0xFAE944,
	0xDFCA24,
	// purple
	0xC48ACB,
	0x7E6583,
	// red
	0xE76F6D,
	0xE13C34,
};

inline uint32_t SampleNativeColor(uint16_t track, uint8_t channel, bool isBlack)
{
	size_t index = (((size_t)track << 4u) | (size_t)channel) << 1;
	index = index % nativeSynthesiaColors.size();
	return nativeSynthesiaColors[index | isBlack];
}

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

	GenerateKeyLayoutArrays();
	CalculateKeyboardData();
	InitializeTextures();
	InitializeParticleSystems();

	initialized = true;
#pragma endregion
}

void MIDIRendererSynthesia::CalculateKeyboardData()
{
	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast() + 1;

	keyboardHeight = initialKeyboardHeight / (keyLast - keyFirst) * 128.0f;
	keyboardHeight = keyboardHeight / (1920.0 / 1080.0) * ((float)this->width / (float)this->height);
}

void MIDIRendererSynthesia::InitializeTextures()
{
	const std::filesystem::path keyboardPath = "./assets/textures/synthesia/keyboards";
	const std::filesystem::path notesPath = "./assets/textures/synthesia/notes";
	const std::filesystem::path particlePath = "./assets/textures/synthesia/particles";

	// layer -> file path, in the same order as the TextureLayer enum.
	const std::array<std::filesystem::path, NUM_TEXTURES> layerPaths = { {
		keyboardPath / "bar.png",
		keyboardPath / "blackKeys.png",
		keyboardPath / "blackKeysPressed.png",
		keyboardPath / "whiteKeys.png",
		keyboardPath / "whiteKeysPressed.png",
		keyboardPath / "whiteKeyWhole.png",
		keyboardPath / "whiteKeyWholePressed.png",
		keyboardPath / "shadowLarge.png",
		keyboardPath / "shadowUnpressed.png",
		keyboardPath / "shadowPressed.png",
		particlePath / "keyDebris.png",
		particlePath / "keySpark.png",
		particlePath / "keyHaze.png"
	} };

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

	textures = std::make_unique<TextureArray>(maxWidth, maxHeight, NUM_TEXTURES);

	for (int layer = 0; layer < NUM_TEXTURES; layer++)
	{
		layerUV[layer] = textures->LoadLayer(layer, Utils::TryGetStream(layerPaths[layer]));
	}

	UpdateStyle();

	{
		ShaderBind bind(*notesProgram);

		{
			TextureBind tex(*noteWhiteBody, 0);
			notesProgram->SetInt("noteWhiteBody", 0);
		}

		{
			TextureBind tex(*noteWhiteTop, 1);
			notesProgram->SetInt("noteWhiteTop", 1);
		}

		{
			TextureBind tex(*noteWhiteBottom, 2);
			notesProgram->SetInt("noteWhiteBottom", 2);
		}

		{
			TextureBind tex(*noteBlackBody, 3);
			notesProgram->SetInt("noteBlackBody", 3);
		}

		{
			TextureBind tex(*noteBlackTop, 4);
			notesProgram->SetInt("noteBlackTop", 4);
		}

		{
			TextureBind tex(*noteBlackBottom, 5);
			notesProgram->SetInt("noteBlackBottom", 5);
		}

		{
			TextureBind tex(*noteOOB, 6);
			notesProgram->SetInt("noteOOB", 6);
		}
	}
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

	const std::array<float, 5> additionalOffsets = { 0.02, -0.035, 0.0, -0.02, -0.03 };
	const float blackKeyScale = 0.63f;
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

void MIDIRendererSynthesia::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
	if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
	AbstractMIDIRenderer::LoadSequence(sequence);

	colors.LoadColors();
	seq = sequence;
	
	lastTime = 0;

	for (auto& id : startRenderIDs)
		id = 0;

	for (auto& id : endRenderIDs)
		id = 0;
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
	std::filesystem::path notesPath;

	switch (renderSettings.style)
	{
		case SYNTHESIA_10:
		{
			notesPath = "./assets/textures/synthesia/notes/s10";
			noteWhiteBody = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "note.png"));
			noteWhiteTop = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteTop.png"));
			noteWhiteBottom = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteBottom.png"));
			noteBlackBody = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "note.png"));
			noteBlackTop = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteTop.png"));
			noteBlackBottom = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteBottom.png"));
			{
				ShaderBind bind(*notesProgram);
				notesProgram->SetFloat("brightnessBoost", 1.0f);
			}
			break;
		}
		case SYNTHESIA_9:
		{
			notesPath = "./assets/textures/synthesia/notes/s9";
			noteWhiteBody = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteWhite.png"));
			noteWhiteTop = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteTopWhite.png"));
			noteWhiteBottom = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteBottomWhite.png"));
			noteBlackBody = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteBlack.png"));
			noteBlackTop = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteTopBlack.png"));
			noteBlackBottom = std::make_unique<GPUImage>(Utils::TryGetStream(notesPath / "noteBottomBlack.png"));
			{
				ShaderBind bind(*notesProgram);
				notesProgram->SetFloat("brightnessBoost", 2.0f);
			}
			break;
		}
	}

	
	noteOOB = std::make_unique<GPUImage>(Utils::TryGetStream("./assets/textures/synthesia/notes/noteOOB.png"));
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
		RenderParticles();
		UpdateParticles(deltaTime);

		RenderQuads(rectDrawCount);
		rectDrawCount = 0;
	}

	RenderKeyboard();
	RenderQuads(rectDrawCount);
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

	float keyTop = keyboardHeight * (1.0f - initialBarHeight);

	const float frac = 1.0f / 7.0f;
	// render white keys first
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
				PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, TextureLayer::LAYER_KEY_WHITE_PRESSED, color);
				PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, TextureLayer::LAYER_KEY_WHITE_WHOLE_PRESSED, color);
			}
			else
			{
				PushQuad(left, 0, middle - left, keyTop, uvLeft, 0, uvMiddle, 1, TextureLayer::LAYER_KEY_WHITE, creamColor);
				PushQuad(middle, 0, right - middle, keyTop, split, 0, 1, 1, TextureLayer::LAYER_KEY_WHITE_WHOLE, creamColor);
			}
		}
		else if (i == keyFirst)
		{
			if (keyStates[i].pressed)
			{
				PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, TextureLayer::LAYER_KEY_WHITE_WHOLE_PRESSED, color);
				PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, TextureLayer::LAYER_KEY_WHITE_PRESSED, color);
			}
			else
			{
				PushQuad(left, 0, middle - left, keyTop, 0, 0, split, 1, TextureLayer::LAYER_KEY_WHITE_WHOLE, creamColor);
				PushQuad(middle, 0, right - middle, keyTop, uvMiddle, 0, uvRight, 1, TextureLayer::LAYER_KEY_WHITE, creamColor);
			}
		}
		else
		{
			if (keyStates[i].pressed)
			{
				PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, TextureLayer::LAYER_KEY_WHITE_PRESSED, color);
			}
			else
			{
				PushQuad(left, 0, right - left, keyTop, uvLeft, 0, uvRight, 1, TextureLayer::LAYER_KEY_WHITE, creamColor);
			}
		}

		#pragma region Shadows
		float aspectRatio = (float)this->width / (float)this->height;
		if (i != 0 && i != keyFirst)
		{
			const auto& RenderLarge = [this, left, keyTop, aspectRatio]()
				{
					float shadowRight = left + keyTop * textures->GetLayerAspectRatio(TextureLayer::LAYER_SHADOW_LARGE) / aspectRatio;
					PushQuad(left, 0, shadowRight - left, keyTop, 0, 0, 1, 1, TextureLayer::LAYER_SHADOW_LARGE);
				};

			const auto& RenderUnpressed = [this, left, keyTop, aspectRatio, i]()
				{
					float keyRight = keyPos[i - 1] + keyWidth[i - 1];
					float shadowRight = keyRight + keyTop * textures->GetLayerAspectRatio(TextureLayer::LAYER_SHADOW_UNPRESSED) / aspectRatio;
					PushQuad(keyRight, 0, shadowRight - keyRight, keyTop, 0, 0, 1, 1, TextureLayer::LAYER_SHADOW_UNPRESSED);
				};

			const auto& RenderPressed = [this, left, keyTop, aspectRatio, i]()
				{
					float keyRight = keyPos[i - 1] + keyWidth[i - 1];
					float shadowRight = keyRight + keyTop * textures->GetLayerAspectRatio(TextureLayer::LAYER_SHADOW_PRESSED) / aspectRatio;
					PushQuad(keyRight, 0, shadowRight - keyRight, keyTop, 0, 0, 1, 1, TextureLayer::LAYER_SHADOW_PRESSED);
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

	PushQuad(0, keyTop, 1, initialBarHeight, 0, 0, 1, 1, TextureLayer::LAYER_BAR);

	for (int i = keyFirst; i <= keyLast; i++)
	{
		if (!IS_BLACK(i)) continue;

		size_t part = keyNum[i] % 5;

		float bKeyBottom = keyboardHeight / 2.9f;
		float bKeyTopPressed = keyTop + keyboardHeight * 0.006;
		float bKeyTopUnpressed = keyTop + keyboardHeight * 0.015;

		float uvLeft = part * (1.0f / 5.0f);
		float uvRight = (part + 1) * (1.0f / 5.0f);

		float left = keyPos[i];
		float right = keyPos[i] + keyWidth[i];

		if (keyStates[i].pressed)
			PushQuad(left, bKeyBottom, right - left, bKeyTopPressed - bKeyBottom, uvLeft, 0, uvRight, 1, TextureLayer::LAYER_KEY_BLACK_PRESSED, 0x7F000000 | keyStates[i].color);
		else
			PushQuad(left, bKeyBottom, right - left, bKeyTopUnpressed - bKeyBottom, uvLeft, 0, uvRight, 1, TextureLayer::LAYER_KEY_BLACK);
	}
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
	if (!seq) return;
	std::vector<NoteSequence>& notes = seq->mergedNotes;
	if (notes.empty()) return;
	auto* renderView = app->GetRenderView();
	if (!renderView) return;

	size_t notesToRender = 0;
	double playbackSeconds = app->GetTimer()->Elapsed();

	TempoMap* tempoMap = seq->GetTempoMap();
	long time = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);
	double bpm = tempoMap->GetBPMAtTick(time);
	noteCounterInfo->tick.value = time >= 0 ? time : 0;
	noteCounterInfo->timeSeconds.value = playbackSeconds;
	noteCounterInfo->bpm.value = bpm;

	ShaderBind shaderBind(*notesProgram);

	TextureBind noteWhiteBodyBind(*noteWhiteBody, 0);
	TextureBind noteWhiteTopBind(*noteWhiteTop, 1);
	TextureBind noteWhiteBottomBind(*noteWhiteBottom, 2);
	TextureBind noteBlackBodyBind(*noteBlackBody, 3);
	TextureBind noteBlackTopBind(*noteBlackTop, 4);
	TextureBind noteBlackBottomBind(*noteBlackBottom, 5);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);
	if (notesIBO) notesIBO->Bind();

	float renderAspectRatio = (float)width / (float)height;
	float capAspectRatio = (float)noteWhiteTop->width / (float)noteWhiteTop->height;
	
	notesProgram->SetFloat("kbHeight", keyboardHeight);
	notesProgram->SetFloat("aspectRatio", renderAspectRatio);
	notesProgram->SetFloat("capHeight", whiteKeyWidth / capAspectRatio * renderAspectRatio);
	notesProgram->SetFloat("capAspectRatio", capAspectRatio);

	size_t noteID = 0;
	size_t notesPassed = 0;
	size_t polyphony = 0;
	size_t batchCount = 0;

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000 : (double)renderView->viewTicks;
	const double invViewRegion = 1.0 / viewRegion;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	MIDIPlayerConfig* config = app->GetConfig();
	int keyFirst = config->render.GetKeyFirst();
	int keyLast = config->render.GetKeyLast();

	for (uint8_t id : kbIDs)
	{
		uint16_t lastTrack = 0;
		uint8_t lastChannel = 0;
		uint32_t lastTick = 0;
		uint32_t lastGate = 0;
		uint32_t lastNote = 0;

		NoteSequence& notesNote = notes[id];

#pragma region Note culling

		size_t noteBegin = startRenderIDs[id];

		if (lastTime < time)
		{
			while (noteBegin < notesNote.Size())
			{
				double noteEnd = isTimeBased
					? (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]) * invTimeMultiplier
					: (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]);

				if (noteEnd > accTime) break;
				++noteBegin;
			}
		}
		else if (lastTime > time)
		{
			while (noteBegin > 0)
			{
				size_t prev = noteBegin - 1;
				double noteEnd = isTimeBased
					? (double)(notesNote.tick[prev] + notesNote.gate[prev]) * invTimeMultiplier
					: (double)(notesNote.tick[prev] + notesNote.gate[prev]);

				if (noteEnd <= accTime) break;
				--noteBegin;
			}
		}

		auto searchStart = notesNote.tick.begin() + noteBegin;
		auto endIt = notesNote.tick.end();

		if (isTimeBased)
		{
			double targetSecs = playbackSeconds + viewRegion;
			long target10Us = static_cast<long>(targetSecs * TIME_BASED_MULTIPLIER);
			endIt = std::upper_bound(searchStart, notesNote.tick.end(), target10Us);
		}
		else
		{
			long targetTick = time + renderView->viewTicks;
			endIt = std::upper_bound(searchStart, notesNote.tick.end(), targetTick);
		}

		size_t noteEnd = std::distance(notesNote.tick.begin(), endIt);
		startRenderIDs[id] = noteBegin;
		endRenderIDs[id] = noteEnd;
		notesPassed += noteBegin;

#pragma endregion

		bool isBlack = IS_BLACK(id);

		for (size_t i = noteBegin; i < noteEnd; ++i)
		{
			uint32_t nTick = notesNote.tick[i];
			uint32_t nGate = notesNote.gate[i];
			uint8_t nNote = notesNote.note[i];
			uint16_t nTrack = notesNote.track[i];
			uint8_t nChannel = notesNote.channel[i];

			double noteStart = isTimeBased ? (double)nTick * invTimeMultiplier : (double)nTick;
			double noteEnd = isTimeBased
				? (double)(nTick + nGate) * invTimeMultiplier
				: (double)(nTick + nGate);

			if (noteStart > accTime + viewRegion) break;
			if (noteEnd <= accTime)
			{
				notesPassed++;
				continue;
			}
			if (noteStart <= accTime)
			{
				keyStates[nNote].pressed = true;
				keyStates[nNote].color = renderSettings.useNativeNoteColors ?
					SampleNativeColor(nTrack, nChannel, isBlack) :
					colors.GetColor(nTrack, nChannel);
				notesPassed++;
				polyphony++;
			}

			if (id < keyFirst || id > keyLast) break;

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

			float x = keyPos[id];
			float width = keyWidth[id];
			float y = (float)((noteStart - accTime) * invViewRegion);
			float y2 = (float)((noteEnd - accTime) * invViewRegion);

			renderNotes[noteID++] = RenderNoteSynthesia(
				x, y,
				width, y2 - y,
				((renderSettings.useNativeNoteColors ?
				SampleNativeColor(nTrack, nChannel, isBlack) :
				colors.GetColor(nTrack, nChannel)) & 0xFFFFFF) | (isBlack << 24)
			);

			if (noteID >= NOTE_BUFFER_SIZE)
			{
				FlushNotes(NOTE_BUFFER_SIZE);
				batchCount++;
				if (batchCount >= NOTES_MAX_BATCHES)
				{
					glFlush();
					batchCount = 0;
				}
				noteID = 0;
			}
		}
	}

	if (noteID != 0)
	{
		FlushNotes(noteID);
		batchCount++;
		if (batchCount >= NOTES_MAX_BATCHES)
		{
			glFlush();
			batchCount = 0;
		}
	}
	
	noteCounterInfo->notesPassed = static_cast<uint64_t>(notesPassed);
	noteCounterInfo->polyphony = static_cast<uint64_t>(polyphony);

	if (!noteCounterInfo->npsHistory.empty() && playbackSeconds < noteCounterInfo->npsHistory.back().timeSeconds)
	{
		noteCounterInfo->npsHistory.clear();
	}

	noteCounterInfo->npsHistory.push_back({ playbackSeconds, static_cast<uint64_t>(notesPassed) });
	while (!noteCounterInfo->npsHistory.empty() &&
		(playbackSeconds - noteCounterInfo->npsHistory.front().timeSeconds) > 1.0)
	{
		noteCounterInfo->npsHistory.pop_front();
	}

	if (!noteCounterInfo->npsHistory.empty())
	{
		uint64_t notesOneSecondAgo = noteCounterInfo->npsHistory.front().totalNotes;
		uint64_t currentNotes = static_cast<uint64_t>(notesPassed);

		if (currentNotes >= notesOneSecondAgo)
		{
			noteCounterInfo->notesPerSecond.value = currentNotes - notesOneSecondAgo;
		}
		else
		{
			noteCounterInfo->notesPerSecond.value = 0;
		}
	}
	else
	{
		noteCounterInfo->notesPerSecond = 0;
	}

	lastTime = time;
}

void MIDIRendererSynthesia::RenderOutOfBoundNotes()
{
	if (!seq) return;
	std::vector<NoteSequence>& notes = seq->mergedNotes;
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

		size_t noteBegin = startRenderIDs[id];
		size_t noteEnd = endRenderIDs[id];

		for (size_t i = noteBegin; i < noteEnd; ++i)
		{
			uint32_t nTick = notesNote.tick[i];
			uint32_t nGate = notesNote.gate[i];
			uint8_t nNote = notesNote.note[i];
			uint16_t nTrack = notesNote.track[i];
			uint8_t nChannel = notesNote.channel[i];

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

void MIDIRendererSynthesia::RenderParticles()
{
	float aspect = (float)width / (float)height;
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

		PushRotatedQuad(pos.x - sizeX * 0.5f, pos.y - sizeY * 0.5f, sizeX, sizeY, p->rotation, 0, 0, 1, 1, LAYER_PARTICLE_DEBRIS);
	}

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
			float sizeX = width * p->size * 2.8f;
			float sizeY = sizeX * aspect;

			if (p->flipped)
			{
				PushRotatedQuad(pos.x - sizeX * 0.5, pos.y - sizeY * 0.5, sizeX, sizeY, ang, 0, 0, 1, 1, LAYER_PARTICLE_SPARKLE, blendCol);
			}
			else
			{
				PushRotatedQuad(pos.x - sizeX * 0.5, pos.y - sizeY * 0.5, sizeX, sizeY, ang, 1, 0, 0, 1, LAYER_PARTICLE_SPARKLE, blendCol);
			}
		}
	}

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

			float sizeX = width * 1.8f;
			float sizeY = sizeX * aspect;

			uint32_t alpha = (uint32_t)(std::clamp(p->brightness, 0.0f, 1.0f) * 0.2f * 127.0f) << 24;
			PushQuad(pos.x - sizeX, pos.y - sizeY, sizeX * 2.0f, sizeY * 2.0f, 0, 0, 1, 1, LAYER_PARTICLE_HAZE, alpha | 0xFFFFFF);
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

	float aspect = (float)width / (float)height;

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

		float fontSize = textHeight * height;
		float offsetY = (right - left) * aspect * 0.25f * height;
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
	if (ImGui::BeginTabBar("##renderSettings"))
	{
		if (ImGui::BeginTabItem("General"))
		{
			BEGIN_SECTION("##sGeneral")
			{
				SETUP_SECTION;

				SECTION_ENTRY(SECTION_LABEL("Synthesia style"),
					{
						auto style = renderSettings.style;
						std::string previewText = "";

						switch (style)
						{
							case SYNTHESIA_9:
								previewText = "Synthesia 9";
								break;
							case SYNTHESIA_10:
								previewText = "Synthesia 10";
								break;
						}

						if (ImGui::BeginCombo("##synthesiaStyle", previewText.c_str()))
						{
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

							ImGui::EndCombo();
						}
					});

				SECTION_ENTRY(TABLE_LABEL_TOOLTIP("Native note colors", "Uses built-in synthesia colors instead of the color palette shared between renderers."),
					{
						if (ImGui::Checkbox("##nativeCols", &renderSettings.useNativeNoteColors))
						{
							ShaderBind shaderBind(*notesProgram);
							notesProgram->SetInt("nativeColors", renderSettings.useNativeNoteColors ? 1 : 0);
						}
					});

				SECTION_ENTRY(SECTION_LABEL("Out-of-bound notes"),
					{
						ImGui::Checkbox("##oobNotes", &renderSettings.showOutOfBoundNotes);
					});

				SECTION_ENTRY(SECTION_LABEL("Background"),
					{
						ImGui::Checkbox("##background", &renderSettings.renderBackground);
					});

				END_SECTION;
			}

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Keyboard"))
		{
			BEGIN_SECTION("##sKeyboard")
			{
				SETUP_SECTION;
				SECTION_ENTRY(SECTION_LABEL("Key sparkles"),
					{
						ImGui::Checkbox("##keySpark", &renderSettings.renderKeySparkle);
					});
				END_SECTION;
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
			BEGIN_SECTION("##sStyle")
			{
				SETUP_SECTION;
				SECTION_ENTRY(SECTION_LABEL("Key octaves"),
					{
						ImGui::Checkbox("##keyOctaves", &renderSettings.showKeyOctaves);
					});
				END_SECTION;
			}
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
	LOAD_VAL(node, "style", styleInt);
	renderSettings.style = static_cast<SynthesiaStyle>(styleInt);

	LOAD_VAL(node, "nativeColors", renderSettings.useNativeNoteColors);
	LOAD_VAL(node, "oobNotes", renderSettings.showOutOfBoundNotes);
	LOAD_VAL(node, "keySparkle", renderSettings.renderKeySparkle);
	LOAD_VAL(node, "background", renderSettings.renderBackground);
	LOAD_VAL(node, "keyOctaves", renderSettings.showKeyOctaves);

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