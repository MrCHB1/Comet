#include "MIDIRenderer.h"
#include "Comet.h"
#include "Render/Renderer/PrimitiveShaders.h"
#include "../App/MIDIApp.h"
#include "../MIDI/TempoMap.h"
#include "Utils.h"
#include <algorithm>

void CheckGLError(const char* label)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cout << "OpenGL Error at " << label << ": " << err << std::endl;
	}
}

const std::vector<float> QUAD_VERTICES = {
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f,
};

const std::vector<unsigned int> QUAD_INDICES = {
	0, 1, 3,
	1, 2, 3
};

void MIDIRenderer::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
	std::lock_guard<std::mutex> lock(renderMutex);

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

void MIDIRenderer::UnloadSequence()
{
	std::lock_guard<std::mutex> lock(renderMutex);
	AbstractMIDIRenderer::UnloadSequence();
}

void MIDIRenderer::LoadResourcePack(std::shared_ptr<ResourcePack> pack, bool loadColors)
{
	if (pack == nullptr)
	{
		std::cout << "Attempt to load a null resource pack." << std::endl;
		return;
	}

	#pragma region Note textures
	auto textureNoteStream = pack->GetStream("note.png");
	textureNote = std::make_unique<GPUImage>(textureNoteStream);

	auto textureNoteBlackStream = pack->GetStream("noteBlack.png");
	textureNoteBlack = textureNoteBlackStream != nullptr
		? std::make_unique<GPUImage>(textureNoteBlackStream)
		: std::make_unique<GPUImage>(pack->GetStream("note.png"));

	auto textureNoteEdgeStream = pack->GetStream("noteEdge.png");
	textureNoteEdge = textureNoteEdgeStream != nullptr
		? std::make_unique<GPUImage>(textureNoteEdgeStream)
		: std::make_unique<GPUImage>(pack->GetStream("note.png"));
	#pragma endregion

	#pragma region Key textures + masks
	textureKeyWhite = std::make_unique<GPUImage>(pack->GetStream("keyWhite.png"));
	textureKeyBlack = std::make_unique<GPUImage>(pack->GetStream("keyBlack.png"));
	textureKeyWhitePressed = std::make_unique<GPUImage>(pack->GetStream("keyWhitePressed.png"));
	textureKeyBlackPressed = std::make_unique<GPUImage>(pack->GetStream("keyBlackPressed.png"));

	LoadMaskTexture(pack.get(), textureKeyWhiteMask, "keyWhiteMask.png");
	LoadMaskTexture(pack.get(), textureKeyBlackMask, "keyBlackMask.png");
	LoadMaskTexture(pack.get(), textureKeyWhiteMaskPressed, "keyWhitePressedMask.png");
	LoadMaskTexture(pack.get(), textureKeyBlackMaskPressed, "keyBlackPressedMask.png");
	#pragma endregion
	// attempt to load note colors
	// TODO: move this to its own function
	if (loadColors)
	{
		auto noteColorStream = pack->GetStream("noteColors.png");
		bool canLoadNoteColors = noteColorStream != nullptr;
		if (!canLoadNoteColors)
		{
#ifdef COMET_DEBUG
			std::cout << "Pack has no noteColors.png. Will use randomized colors" << std::endl;
#endif
			colors.ResetColors();
		}
		else
		{
			bool loopColors = pack->GetNoteInfo()->loopColors;
			colors.LoadColors(noteColorStream, loopColors);
#ifdef COMET_DEBUG
			std::cout << "Loaded " << colors.GetNumLoadedColors() << " color(s) with looping " << (loopColors ? "enabled" : "disabled") << std::endl;
#endif
		}
	}

	this->pack = pack;

	if (initialized) CalcKeyPosAndWidth();
	std::cout << "Loaded pack " << pack->GetName() << std::endl;
}

void MIDIRenderer::LoadMaskTexture(ResourcePack* pack, std::unique_ptr<GPUImage>& mask, const char* name)
{
	// load masks and fallback if they dont exist (backwards compatibility woah)
	std::vector<unsigned char> fallbackMask{ 255, 0, 0, 255 };

	mask = std::make_unique<GPUImage>(pack->GetStream(name));
	if (!mask->IsValidTexture())
	{
		mask = std::make_unique<GPUImage>(fallbackMask, 1, 1);
	}
}

void MIDIRenderer::Initialize()
{
	static_assert(sizeof(RenderKeyboardKey) == 12);
	static_assert(offsetof(RenderKeyboardKey, left) == 0);
	static_assert(offsetof(RenderKeyboardKey, right) == 4);
	static_assert(offsetof(RenderKeyboardKey, meta) == 8);

	AbstractMIDIRenderer::Initialize();

#pragma region Load resource pack
	// we can load the resource pack
	auto config = app->GetConfig();
	auto defaultPack = DefaultResourcePack::Instance();
	if (auto pack = std::dynamic_pointer_cast<ResourcePack>(defaultPack))
	{
		LoadResourcePack(pack, config->render.GetUseColorsFromImage());
	}
#pragma endregion

#pragma region Keyboard shader creation

	std::unique_ptr<ShaderProgram> kbProgram = ShaderProgram::CreateFromFiles("assets/shaders/keyboard");
	std::unique_ptr<VertexArray> kbVao = std::make_unique<VertexArray>();
	std::unique_ptr<Buffer> kbVbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> kbInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> kbEbo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*kbVao);

		// static quad verts
		kbVbo->Bind();
		kbVbo->SetData(QUAD_VERTICES, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

		// index buffer
		kbEbo->Bind();
		kbEbo->SetData(QUAD_INDICES, GL_STATIC_DRAW);

		// instance buffer
		kbInstance->Bind();
		kbInstance->SetData(keyboardData, GL_DYNAMIC_DRAW);

		kbVao->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey), offsetof(RenderKeyboardKey, left));
		kbVao->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey), offsetof(RenderKeyboardKey, right));
		kbVao->SetIntAttribute(3, 1, sizeof(RenderKeyboardKey), offsetof(RenderKeyboardKey, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
	}

#pragma endregion

#pragma region Note shader creation

	std::unique_ptr<ShaderProgram> notesProgram = ShaderProgram::CreateFromFiles("assets/shaders/notes");
	std::unique_ptr<VertexArray> notesVao = std::make_unique<VertexArray>();
	std::unique_ptr<Buffer> notesVbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> notesInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> notesEbo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*notesVao);

		// static quad verts
		notesVbo->Bind();
		notesVbo->SetData(QUAD_VERTICES, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

		// index buffer
		notesEbo->Bind();
		notesEbo->SetData(QUAD_INDICES, GL_STATIC_DRAW);

		// instance buffer
		notesInstance->Bind();
		notesInstance->SetData(renderNotes, GL_DYNAMIC_DRAW);

		notesVao->SetFloatAttribute(1, 1, sizeof(RenderNote), offsetof(RenderNote, left));
		notesVao->SetFloatAttribute(2, 1, sizeof(RenderNote), offsetof(RenderNote, right));
		notesVao->SetFloatAttribute(3, 1, sizeof(RenderNote), offsetof(RenderNote, start));
		notesVao->SetFloatAttribute(4, 1, sizeof(RenderNote), offsetof(RenderNote, end));
		notesVao->SetIntAttribute(5, 1, sizeof(RenderNote), offsetof(RenderNote, color));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
	}

#pragma endregion

	std::array<KeyboardMeta, MIDI_KEYS> kbMetas;
	kbMetas.fill(KeyboardMeta(0, false, false));
	std::vector<uint8_t> blackIDs;
	blackIDs.reserve(53);
	std::vector<uint8_t> whiteIDs;
	whiteIDs.reserve(75);

	uint8_t keyID = 0;
	for (uint8_t key = 0; key < MIDI_KEYS; key++)
	{
		bool black = KEY_IS_BLACK(key);
		if (black) blackIDs.push_back(key);
		else whiteIDs.push_back(key);
		kbMetas[key].MarkBlack(black);
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

	// bind textures
	{
		ShaderBind kbBind(*kbProgram);

		{
			TextureBind kbWhiteBind(*textureKeyWhite, 0);
			kbProgram->SetInt("whiteKey", 0);
		}

		{
			TextureBind kbBlackBind(*textureKeyBlack, 1);
			kbProgram->SetInt("blackKey", 1);
		}

		{
			TextureBind kbWhitePressedBind(*textureKeyWhitePressed, 2);
			kbProgram->SetInt("whiteKeyPressed", 2);
		}

		{
			TextureBind kbBlackPressedBind(*textureKeyBlackPressed, 3);
			kbProgram->SetInt("blackKeyPressed", 3);
		}

		{
			TextureBind kbWhiteMaskBind(*textureKeyWhiteMask, 4);
			kbProgram->SetInt("whiteKeyMask", 4);
		}

		{
			TextureBind kbBlackMaskBind(*textureKeyBlackMask, 5);
			kbProgram->SetInt("blackKeyMask", 5);
		}

		{
			TextureBind kbWhiteMaskPressedBind(*textureKeyWhiteMaskPressed, 6);
			kbProgram->SetInt("whiteKeyPressedMask", 6);
		}

		{
			TextureBind kbBlackMaskPressedBind(*textureKeyBlackMaskPressed, 7);
			kbProgram->SetInt("blackKeyPressedMask", 7);
		}
	}

#pragma region Keyboard data setup
	keyboardProgram = std::move(kbProgram);
	keyboardVBO = std::move(kbVbo);
	keyboardVAO = std::move(kbVao);
	keyboardIBO = std::move(kbInstance);
	keyboardEBO = std::move(kbEbo);
	keyMetas = std::move(kbMetas);

	keyboardBackground = std::make_unique<Quad>();
	keyboardBackground->SetShader(SINGLE_COLOR_SHADER);
	keyboardBackground->SetColor(glm::vec3(0.0, 0.0, 0.0));
#pragma endregion

#pragma region Note data setup

	{
		ShaderBind notesBind(*notesProgram);
		notesProgram->SetFloat("kbHeight", keyboardHeight);

		{
			TextureBind noteTextureBind(*textureNote, 0);
			notesProgram->SetInt("note", 0);
		}

		{
			TextureBind noteBlackTextureBind(*textureNoteBlack, 1);
			notesProgram->SetInt("noteBlack", 1);
		}

		{
			TextureBind noteEdgeTextureBind(*textureNoteEdge, 2);
			notesProgram->SetInt("noteEdge", 2);
		}
	}

	this->notesProgram = std::move(notesProgram);
	this->notesVBO = std::move(notesVbo);
	this->notesVAO = std::move(notesVao);
	this->notesIBO = std::move(notesInstance);
	this->notesEBO = std::move(notesEbo);
#pragma endregion

	initialized = true;

	CalcKeyPosAndWidth();
	UpdateKeyboardInstance();
	keyboardDirty = true;

	InitializeFromConfig();
}

void MIDIRenderer::InitializeFromConfig()
{
	auto config = app->GetConfig();

	ImVec4 barColor = config->render.GetBarColor();
	SetBarColor(barColor.x, barColor.y, barColor.z);

	ImVec4 bgColor = config->render.GetBackground();
	SetBackgroundColor(bgColor.x, bgColor.y, bgColor.z);
}

void MIDIRenderer::Render(double deltaTime)
{
	if (!initialized) return;

	sceneFramebuffer->Bind();
	// lowkey forgot to clear the framebuffer at the start of each render, oops
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	RenderNotes();
	RenderKeyboard();
	sceneFramebuffer->Unbind();

	if (!seq)
		noteCounterInfo->tick.value = 0;
}

void MIDIRenderer::RenderSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();

	if (ImGui::BeginTabBar("##midiRenderSettings"))
	{
		if (ImGui::BeginTabItem("Piano"))
		{
			auto colors = config->render.GetBarColor();
			float barColor[3]{ colors.x, colors.y, colors.z };
			ImGui::Text("Bar color");
			ImGui::SameLine();
			if (ImGui::ColorEdit3("##barColor", barColor))
			{
				config->render.SetBarColor(barColor[0], barColor[1], barColor[2]);
				SetBarColor(barColor[0], barColor[1], barColor[2]);
			}

			colors = config->render.GetBackground();
			float bgColor[3]{ colors.x, colors.y, colors.z };
			ImGui::Text("Background color");
			ImGui::SameLine();
			if (ImGui::ColorEdit3("##bgColor", bgColor))
			{
				config->render.SetBackground(bgColor[0], bgColor[1], bgColor[2]);
				SetBackgroundColor(bgColor[0], bgColor[1], bgColor[2]);
			}

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Resource Packs"))
		{
			ResourcePackList* packList = app->GetPackList();
			auto* config = app->GetConfig();
			if (packList != nullptr)
			{
				if (ImGui::Button("Refresh Pack List"))
				{
					packList->RefreshList();
					std::shared_ptr<ResourcePack> activePack = packList->GetActivePack();
					LoadResourcePack(activePack, !config->render.GetUseColorsFromImage());
				}
				ImGui::SameLine();
				if (ImGui::Button("Open pack folder"))
				{
					Utils::OpenFolder(RESOURCE_PACK_FOLDER);
				}

				ImGui::Separator();

				auto& packs = packList->GetPackList();
				size_t packIdx = 0;
				for (const auto& pack : packs)
				{
					ImGui::BeginGroup();
					ImGui::Text(pack->GetName());
					ImGui::Text("By %s", pack->GetAuthor());
					ImGui::Text(pack->GetDescription());

					bool isActivePack = pack == packList->GetActivePack();
					ImGui::BeginDisabled(isActivePack);
					if (isActivePack) ImGui::Button("Pack in use");
					else
					{
						ImGui::PushID(pack.get());
						if (ImGui::Button("Use pack"))
						{
							packList->SwitchPack(packIdx);
							LoadResourcePack(pack, !config->render.GetUseColorsFromImage());
						}
						ImGui::PopID();
					}
					ImGui::EndDisabled();

					ImGui::EndGroup();
					if (packIdx != packs.size() - 1)
						ImGui::Separator();

					packIdx++;
				}
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	AbstractMIDIRenderer::RenderSettings();
}

void MIDIRenderer::ResetSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();
	config->render.SetBarColor(0.5f, 0.f, 0.f);
	SetBarColor(0.5f, 0.f, 0.f);
	config->render.SetBackground(0.f, 0.f, 0.f);
	SetBackgroundColor(0.f, 0.f, 0.f);
}

void MIDIRenderer::CalcKeyPosAndWidth()
{
	float keyboardHeightScale = width / 75.0f / (float)textureKeyWhite->width;
	keyboardHeightBlack = (textureKeyBlack->height * keyboardHeightScale) / (float)height;
	keyboardHeightWhite = (textureKeyWhite->height * keyboardHeightScale) / (float)height;
	keyboardHeight = std::max(keyboardHeightBlack, keyboardHeightWhite) + 1.0f / float(height);
	float noteWidth = (float)width / 75.0f;
	float noteWidthBlack = (float)width / 115.0f;
	float pos = 0.0f;
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		keyPos[i] = pos / (float)width;
		pos += keyPosDiff[i % 12] * noteWidth;
	}
	int lastIdxWhite = -1;
	for (int j = 0; j < MIDI_KEYS; j++)
	{
		if (KEY_IS_BLACK(j))
			keyWidth[j] = noteWidthBlack / (float)width;
		else
		{
			if (lastIdxWhite != -1)
				keyWidth[lastIdxWhite] = keyPos[j] - keyPos[lastIdxWhite];
			lastIdxWhite = j;
		}
	}
	// keyWidth[lastIdxWhite] = ((float)(width) - keyPos[lastIdxWhite]) / (float)width;
	if (lastIdxWhite != -1)
	{
		keyWidth[lastIdxWhite] = 1.0f - keyPos[lastIdxWhite];
	}
	float widthScale = (float)(width) / 1280.0f;
	float unscaledWhiteKeyGap = pack->GetKeyboardInfo()->whiteKeyGap;
	if (unscaledWhiteKeyGap > 0.0f)
	{
		whiteKeyGap = (float)std::max(1, (int)std::floor(unscaledWhiteKeyGap * widthScale));
	}
	else
	{
		whiteKeyGap = 0.0f;
	}

	float unscaledNoteBorderWidth = pack->GetNoteInfo()->borderWidth;
	if (unscaledNoteBorderWidth > 0.0f)
		noteBorderWidth = (float)std::max(1, (int)std::floor(unscaledNoteBorderWidth * widthScale)) / (float)height;
	else
		noteBorderWidth = 0.0f;

	keyboardDirty = true;
}

void MIDIRenderer::UpdateKeyboardInstance()
{
	if (!keyboardIBO)
	{
		std::cout << "Instance buffer doesn't exist yet" << std::endl;
		return;
	}

	if (!keyboardDirty) return;

	// for (int i = 0; i < 128; i++)
	int i = 0;
	for (uint8_t id : kbIDs)
	{
		float left = keyPos[id];
		float right = keyPos[id] + keyWidth[id];
		keyboardData[i].left = left;
		keyboardData[i].right = right;
		keyboardData[i].meta = keyMetas[id].GetMeta();
		i++;
	}

	{
		keyboardProgram->SetFloat("kbWhiteHeight", keyboardHeightWhite);
		keyboardProgram->SetFloat("kbBlackHeight", keyboardHeightBlack);
		keyboardProgram->SetFloat("kbHeight", keyboardHeight - 1.0f / (float)height);
		keyboardProgram->SetFloat("whiteKeyBorderPixelWidth", whiteKeyGap);
		keyboardProgram->SetFloat("borderPercentageNorm", (float)pack->GetKeyboardInfo()->whiteKeyBorderPixels / (float)textureKeyWhite->width);
	}

	keyboardIBO->Bind();
	glBufferSubData(GL_ARRAY_BUFFER,
		0,
		keyboardData.size() * sizeof(RenderKeyboardKey),
		keyboardData.data());

	keyboardBackground->SetTransform({ { 0.0, 0.0, 0.0 }, { 1.0f, keyboardHeight } });
	keyboardDirty = false;
}

void MIDIRenderer::OnResize(int width, int height)
{
	this->width = width;
	this->height = height;
	AbstractMIDIRenderer::OnResize(width, height);
	if (!initialized) return;
	CalcKeyPosAndWidth();
}

void MIDIRenderer::RenderKeyboard()
{
	keyboardBackground->Draw();

	{
		ShaderBind kbBind(*keyboardProgram);

		TextureBind kbWhiteBind(*textureKeyWhite, 0);
		TextureBind kbBlackBind(*textureKeyBlack, 1);
		TextureBind kbWhitePressedBind(*textureKeyWhitePressed, 2);
		TextureBind kbBlackPressedBind(*textureKeyBlackPressed, 3);
		TextureBind kbWhiteMaskBind(*textureKeyWhiteMask, 4);
		TextureBind kbBlackMaskBind(*textureKeyBlackMask, 5);
		TextureBind kbWhiteMaskPressedBind(*textureKeyWhiteMaskPressed, 6);
		TextureBind kbBlackMaskPressedBind(*textureKeyBlackMaskPressed, 7);

		VertexArrayBind kbVAOBind(*keyboardVAO);
		BufferBind kbIBOBind(*keyboardIBO);
		BufferBind kbVBOBind(*keyboardVBO);
		BufferBind kbEBOBind(*keyboardEBO);

		UpdateKeyboardInstance();

		glDrawElementsInstanced(
			GL_TRIANGLES,
			6,
			GL_UNSIGNED_INT,
			nullptr,
			MIDI_KEYS
		);

		// after drawing reset keyboard state
		for (KeyboardMeta& kbMeta : keyMetas)
		{
			if (kbMeta.pressed)
			{
				kbMeta.MarkPressed(false);
				keyboardDirty = true;
			}
		}
	}
}

void MIDIRenderer::RenderNotes()
{
	if (!seq) return;
	std::vector<std::vector<NoteEvent>>& notes = seq->mergedNotes;
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

	TextureBind noteTextureBind(*textureNote, 0);
	TextureBind noteBlackTextureBind(*textureNoteBlack, 1);
	TextureBind noteEdgeTextureBind(*textureNoteEdge, 2);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesIBOBind(*notesIBO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);

	notesProgram->SetFloat("kbHeight", keyboardHeight - 1.0f / (float)height);
	notesProgram->SetFloat("noteBorderWidth", noteBorderWidth);

	size_t noteID = 0;
	size_t notesPassed = 0;
	size_t polyphony = 0;
	size_t batchCount = 0;

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000 : (double)renderView->viewTicks;
	const double invViewRegion = 1.0 / viewRegion;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	for (uint8_t id : kbIDs)
	{
		std::vector<NoteEvent>& notesNote = notes[id];

#pragma region Note culling

		size_t noteBegin = startRenderIDs[id];

		if (lastTime < time)
		{
			while (noteBegin < notesNote.size())
			{
				auto& n = notesNote[noteBegin];
				double noteEnd = isTimeBased
					? (double)(n.tick + n.gate) * invTimeMultiplier
					: (double)(n.tick + n.gate);

				if (noteEnd > accTime) break; // Note is still on screen
				++noteBegin;
			}
		}
		else if (lastTime > time)
		{
			while (noteBegin > 0)
			{
				auto& n = notesNote[noteBegin - 1];
				double noteEnd = isTimeBased
					? (double)(n.tick + n.gate) * invTimeMultiplier
					: (double)(n.tick + n.gate);

				if (noteEnd <= accTime) break;
				--noteBegin;
			}
		}

		auto searchStart = notesNote.begin() + noteBegin;
		auto endIt = notesNote.end();

		if (isTimeBased)
		{
			double targetSecs = playbackSeconds + viewRegion;
			long target10Us = static_cast<long>(targetSecs * TIME_BASED_MULTIPLIER);

			endIt = std::upper_bound(searchStart, notesNote.end(), target10Us,
				[](long target, const NoteEvent& n)
				{
					return target < n.tick;
				}
			);
		}
		else
		{
			long targetTick = time + renderView->viewTicks;
			endIt = std::upper_bound(searchStart, notesNote.end(), targetTick,
				[](long target, const NoteEvent& n)
				{
					return target < n.tick;
				}
			);
		}

		size_t noteEnd = endIt - notesNote.begin();
		startRenderIDs[id] = noteBegin;
		endRenderIDs[id] = noteEnd;
		notesPassed += noteBegin;

#pragma endregion

		bool isBlack = KEY_IS_BLACK(id);
		// actually render each note
		for (auto note = notesNote.begin() + noteBegin; note != notesNote.begin() + noteEnd; ++note)
		{
			auto& n = *note;
			double noteStart = 0.0;
			double noteEnd = 0.0;

			if (isTimeBased)
			{
				noteStart = (double)note->tick * invTimeMultiplier;
				noteEnd = (double)(note->tick + note->gate) * invTimeMultiplier;
			}
			else
			{
				noteStart = note->tick;
				noteEnd = note->tick + note->gate;
			}

			if (noteEnd <= accTime)
			{
				notesPassed++;
				continue;
			}
			if (noteStart <= accTime)
			{
				keyMetas[n.note].MarkPressed(true);
				keyMetas[n.note].color = colors.GetColor(note->track, note->channel);
				keyboardDirty = true;

				notesPassed++;
				polyphony++;
			}

			renderNotes[noteID++] = RenderNote(
				keyPos[id],
				keyPos[id] + keyWidth[id],
				(float)((noteStart - accTime) * invViewRegion),
				(float)((noteEnd - accTime) * invViewRegion),
				colors.GetColor(note->track, note->channel) | (isBlack << 24)
			);

			if (noteID >= NOTE_BUFFER_SIZE)
			{
				UploadNoteBuffer(NOTE_BUFFER_SIZE);
				batchCount++;
				if (batchCount >= NOTES_MAX_BATCHES)
				{
					glFlush();
					batchCount = 0;
				}
				noteID = 0;
			}
			notesToRender++;
		}
	}

	if (noteID != 0)
	{
		UploadNoteBuffer(noteID);
		batchCount++;
	}

	noteCounterInfo->notesPassed.value = static_cast<uint64_t>(notesPassed);
	noteCounterInfo->polyphony.value = static_cast<uint64_t>(polyphony);

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
		noteCounterInfo->notesPerSecond.value = static_cast<uint64_t>(notesPassed) - notesOneSecondAgo;
	}
	else
	{
		noteCounterInfo->notesPerSecond.value = 0;
	}

	lastTime = time;
}

void MIDIRenderer::UploadNoteBuffer(size_t count)
{
	if (!notesIBO) return;

	notesIBO->Bind();

	glBufferSubData(GL_ARRAY_BUFFER,
		0,
		count * sizeof(RenderNote),
		renderNotes.data());

	glDrawElementsInstanced(
		GL_TRIANGLES,
		6,
		GL_UNSIGNED_INT,
		nullptr,
		count
	);
}