#include "MIDIRenderer.h"
#include "Comet.h"
#include "../App/MIDIApp.h"
#include "../MIDI/TempoMap.h"

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

	if (seq != sequence) seq.reset();

	colors.LoadColors();
	seq = sequence;
}

void MIDIRenderer::UnloadSequence()
{
	std::lock_guard<std::mutex> lock(renderMutex);

	seq = nullptr;
}

void MIDIRenderer::LoadResourcePack(ResourcePack* pack)
{
	if (pack == nullptr)
	{
		std::cout << "Attempt to load a null resource pack." << std::endl;
		return;
	}

	textureNote = std::make_unique<GPUImage>(*(pack->GetStream("note.png").get()));
	textureNoteEdge = std::make_unique<GPUImage>(*(pack->GetStream("noteEdge.png").get()));
	textureKeyWhite = std::make_unique<GPUImage>(*(pack->GetStream("keyWhite.png").get()));
	textureKeyBlack = std::make_unique<GPUImage>(*(pack->GetStream("keyBlack.png").get()));
	textureKeyWhitePressed = std::make_unique<GPUImage>(*(pack->GetStream("keyWhitePressed.png").get()));
	textureKeyBlackPressed = std::make_unique<GPUImage>(*(pack->GetStream("keyBlackPressed.png").get()));

	std::cout << "Loaded pack " << pack->GetName() << std::endl;

	this->pack = pack;
}

void MIDIRenderer::Initialize()
{
	static_assert(sizeof(RenderKeyboardKey) == 12);
	static_assert(offsetof(RenderKeyboardKey, left) == 0);
	static_assert(offsetof(RenderKeyboardKey, right) == 4);
	static_assert(offsetof(RenderKeyboardKey, meta) == 8);

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

	std::array<KeyboardMeta, MIDI_KEYS> kbMetas(KeyboardMeta(0, false, false));
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
	}

	#pragma region Keyboard data setup
	keyboardProgram = std::move(kbProgram);
	keyboardVBO = std::move(kbVbo);
	keyboardVAO = std::move(kbVao);
	keyboardIBO = std::move(kbInstance);
	keyboardEBO = std::move(kbEbo);
	keyMetas = std::move(kbMetas);

	keyboardBackground = std::make_unique<Quad>();
	auto bgColor = pack->GetKeyboardInfo()->background;
	keyboardBackground->SetColor(glm::vec3(bgColor.x, bgColor.y, bgColor.z));

	CalcKeyPosAndWidth();
	UpdateKeyboardInstance();
	keyboardDirty = true;
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
			TextureBind noteEdgeTextureBind(*textureNoteEdge, 1);
			notesProgram->SetInt("noteEdge", 1);
		}
	}

	this->notesProgram = std::move(notesProgram);
	this->notesVBO = std::move(notesVbo);
	this->notesVAO = std::move(notesVao);
	this->notesIBO = std::move(notesInstance);
	this->notesEBO = std::move(notesEbo);
	#pragma endregion
	
	initialized = true;
}

void MIDIRenderer::Render()
{
	if (!initialized) return;
	RenderNotes();
	RenderKeyboard();
}

void MIDIRenderer::CalcKeyPosAndWidth()
{
	float keyboardHeightScale = width / 75.0f / (float)textureKeyWhite->width;
	keyboardHeightBlack = (textureKeyBlack->height * keyboardHeightScale) / (float)height;
	keyboardHeightWhite = (textureKeyWhite->height * keyboardHeightScale) / (float)height;
	keyboardHeight = std::max(keyboardHeightBlack, keyboardHeightWhite) + 2.0f / float(height);
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
	keyWidth[(int)(keyWidth.size() - 1)] = ((float)(width) - keyPos[lastIdxWhite]) / (float)width;
	float widthScale = (float)(width) / 1280.0f;
	float unscaledWhiteKeyGap = pack->GetKeyboardInfo()->whiteKeyGap;
	if (unscaledWhiteKeyGap > 0.0f)
	{
		whiteKeyGap = (float)std::max(1, (int)std::floor(unscaledWhiteKeyGap * widthScale)) / (float)width;
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
		keyboardProgram->SetFloat("kbHeight", keyboardHeight - 2.0f / (float)height);
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
	long time = seq->GetTempoMap()->SecsToTicksFromMap(seq->resolution, app->GetTimer()->Elapsed());

	ShaderBind shaderBind(*notesProgram);

	TextureBind noteTextureBind(*textureNote, 0);
	TextureBind noteEdgeTextureBind(*textureNoteEdge, 1);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesIBOBind(*notesIBO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);

	notesProgram->SetFloat("kbHeight", keyboardHeight - 2.0f / (float)height);
	notesProgram->SetFloat("noteBorderWidth", noteBorderWidth);

	size_t noteID = 0;
	for (uint8_t id : kbIDs)
	{
		std::vector<NoteEvent>& notesNote = notes[id];

		#pragma region Note culling

		size_t noteBegin = startRenderIDs[id];
		if (lastTime < time)
		{
			while (noteBegin < notesNote.size() && notesNote[noteBegin].tick + notesNote[noteBegin].gate <= time)
			{
				++noteBegin;
			}
		}
		else if (lastTime > time)
		{
			while (noteBegin > 0 &&
				notesNote[noteBegin - 1].tick + notesNote[noteBegin - 1].gate > time)
			{
				--noteBegin;
			}
		}
		
		auto endIt = std::upper_bound(
			notesNote.begin() + noteBegin,
			notesNote.end(),
			time + renderView->viewTicks,
			[](long tick, const NoteEvent& n)
			{
				return tick < n.tick;
			}
		);

		size_t noteEnd = endIt - notesNote.begin();
		startRenderIDs[id] = noteBegin;
		endRenderIDs[id] = noteEnd;

		#pragma endregion

		// actually render each note
		for (auto note = notesNote.begin() + noteBegin; note != notesNote.begin() + noteEnd; ++note)
		{
			auto& n = *note;
			if (n.tick + n.gate <= time) continue;
			if (n.tick <= time)
			{
				keyMetas[n.note].MarkPressed(true);
				keyMetas[n.note].color = colors.GetColor(note->track, note->channel);
				keyboardDirty = true;
			}

			renderNotes[noteID++] = RenderNote(
				keyPos[id],
				keyPos[id] + keyWidth[id],
				(float)(n.tick - time) / (float)renderView->viewTicks,
				(float)(n.tick + n.gate - time) / (float)renderView->viewTicks,
				colors.GetColor(note->track, note->channel)
			);
			
			if (noteID >= NOTE_BUFFER_SIZE)
			{
				UploadNoteBuffer(NOTE_BUFFER_SIZE);
				noteID = 0;
			}
			notesToRender++;
		}
	}

	if (noteID != 0)
	{
		UploadNoteBuffer(noteID);
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