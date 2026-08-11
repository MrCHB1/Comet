#include "MIDIRendererSynthesia.h"
#include "Utils.h"
#include "App/MIDIApp.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"
#include "RenderView.h"
#include <memory>
#include <algorithm>

inline constexpr const char* rectVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 rectPos;
layout (location = 2) in vec2 rectSize;
layout (location = 3) in vec2 uv0;
layout (location = 4) in vec2 uv1;
layout (location = 5) in int textureIndex;
layout (location = 6) in uint aMeta; 

flat out uint meta;
flat out int texID;

out vec2 uv;

void main()
{
	meta = aMeta;
	texID = textureIndex;

	uv = mix(uv0, uv1, aPos);

    vec2 pos = rectPos + aPos * rectSize;
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

void MIDIRendererSynthesia::Initialize()
{
	AbstractMIDIRenderer::Initialize();

	InitializeTextures();

#pragma region Rect shader initialization
	rectProgram = ShaderProgram::Create(rectVert, rectFrag);
	rectVAO = std::make_unique<VertexArray>();
	rectVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	rectIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	rectEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*rectVAO);

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
		rectVAO->SetFloatAttribute(3, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, uv0));
		rectVAO->SetFloatAttribute(4, 2, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, uv1));
		rectVAO->SetIntAttribute(5, 1, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, textureIndex));
		rectVAO->SetIntAttribute(6, 1, sizeof(TexturedRectInstance), offsetof(TexturedRectInstance, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);
	}

	// The sampler uniform only needs to be set once - it just points at a
	// texture unit, and we always bind the array to that same unit.
	{
		ShaderBind shaderBind(*rectProgram);
		rectProgram->SetInt("textures", TEXTURE_ARRAY_SLOT);
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

	// layer -> file path, in the same order as the TextureLayer enum.
	const std::array<std::filesystem::path, NUM_TEXTURES> layerPaths = { {
		keyboardPath / "bar.png",
		keyboardPath / "blackKeys.png",
		keyboardPath / "blackKeysPressed.png",
		keyboardPath / "whiteKeys.png",
		keyboardPath / "whiteKeysPressed.png",
		keyboardPath / "whiteKeyWhole.png",
		keyboardPath / "whiteKeyWholePressed.png",
		// notesPath / "noteTop.png",
		// notesPath / "noteBottom.png",
		// notesPath / "note.png",
		keyboardPath / "shadowLarge.png",
		keyboardPath / "shadowUnpressed.png",
		keyboardPath / "shadowPressed.png",
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

void MIDIRendererSynthesia::Render(double deltaTime)
{
	rectDrawCount = 0;

	UpdateRenderer();

	sceneFramebuffer->Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// if background drawing
	RenderBackground();
	RenderLines();
	RenderQuads(rectDrawCount);
	rectDrawCount = 0;

	RenderNotes();
	RenderKeyboard();
	RenderQuads(rectDrawCount);
	

	glDisable(GL_BLEND);

	sceneFramebuffer->Unbind();
}

void MIDIRendererSynthesia::RenderBackground()
{
	PushQuad(0.0, 0.0, 1.0, 1.0, 0xFF252525);
}

void MIDIRendererSynthesia::RenderLines()
{
	MIDIPlayerConfig* config = app->GetConfig();
	const int keyFirst = config->render.GetKeyFirst();
	const int keyLast = config->render.GetKeyLast() + 1;

	const float lineWidth = 0.02f;

	for (int i = keyFirst; i <= keyLast; i++)
	{
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

	// reset keyboard state here
	for (int i = keyFirst; i <= keyLast; i++)
	{
		keyStates[i].pressed = false;
	}
}

void MIDIRendererSynthesia::RenderQuads(size_t count)
{
	if (count == 0) return;

	ShaderBind shaderBind(*rectProgram);
	VertexArrayBind vaoBind(*rectVAO);
	BufferBind iboBind(*rectIBO);
	TextureArrayBind texBind(*textures, TEXTURE_ARRAY_SLOT);

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
		size_t noteEnd = endRenderIDs[id];

		if (lastTime != time)
		{
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

			noteEnd = std::distance(notesNote.tick.begin(), endIt);
			startRenderIDs[id] = noteBegin;
			endRenderIDs[id] = noteEnd;
		}

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
				keyStates[nNote].color = colors.GetColor(nTrack, nChannel);
				notesPassed++;
				polyphony++;
			}

			if (id < keyFirst || id >= keyLast) break;

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

			// TODO: actual note rendering
		}
	}
	
	lastTime = time;
}

void MIDIRendererSynthesia::RenderSettings()
{
	AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererSynthesia::ResetSettings()
{

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

	return node;
}

void MIDIRendererSynthesia::LoadSettings(const YAML::Node& node)
{

}