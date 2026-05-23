#include "MIDIRenderer.h"
#include "UMPRevanced.h"

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

	// test: verify packs were actually loaded
#ifdef COMET_DEBUG
	std::cout << "textureNote | res: (" << textureNote->width << ", " << textureNote->height << ")" << std::endl;
	std::cout << "textureNoteEdge | res: (" << textureNoteEdge->width << ", " << textureNoteEdge->height << ")" << std::endl;
	std::cout << "textureKeyWhite | res: (" << textureKeyWhite->width << ", " << textureKeyWhite->height << ")" << std::endl;
	std::cout << "textureKeyBlack | res: (" << textureKeyBlack->width << ", " << textureKeyBlack->height << ")" << std::endl;
	std::cout << "textureKeyWhitePressed | res: (" << textureKeyWhitePressed->width << ", " << textureKeyWhitePressed->height << ")" << std::endl;
	std::cout << "textureKeyBlackPressed | res: (" << textureKeyBlackPressed->width << ", " << textureKeyBlackPressed->height << ")" << std::endl;
#endif

	this->pack = pack;
}

void MIDIRenderer::Initialize()
{
	static_assert(sizeof(RenderKeyboardKey) == 12);
	static_assert(offsetof(RenderKeyboardKey, left) == 0);
	static_assert(offsetof(RenderKeyboardKey, right) == 4);
	static_assert(offsetof(RenderKeyboardKey, meta) == 8);

	std::unique_ptr<ShaderProgram> kbProgram = ShaderProgram::CreateFromFiles("assets/shaders/keyboard");

	keyboardData = std::vector<RenderKeyboardKey>(128, { 0.0f, 0.5f, 0 });

	std::unique_ptr<VertexArray> vao = std::make_unique<VertexArray>();
	std::unique_ptr<Buffer> kbVbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> kbInstance = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	std::unique_ptr<Buffer> kbEbo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*vao);

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

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(
			1, 1, GL_FLOAT, GL_FALSE,
			sizeof(RenderKeyboardKey),
			(void*)offsetof(RenderKeyboardKey, left)
		);
		glVertexAttribDivisor(1, 1);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(
			2, 1, GL_FLOAT, GL_FALSE,
			sizeof(RenderKeyboardKey),
			(void*)offsetof(RenderKeyboardKey, right)
		);
		glVertexAttribDivisor(2, 1);

		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(
			3, 1, GL_UNSIGNED_INT,
			sizeof(RenderKeyboardKey),
			(void*)offsetof(RenderKeyboardKey, meta)
		);
		glVertexAttribDivisor(3, 1);
	}

	std::vector<KeyboardMeta> kbMetas;
	kbMetas.reserve(128);
	std::vector<uint8_t> blackIDs;
	blackIDs.reserve(53);
	std::vector<uint8_t> whiteIDs;
	whiteIDs.reserve(75);

	for (uint8_t key = 0; key < 128; key++)
	{
		bool black = KEY_IS_BLACK(key);
		if (black) blackIDs.push_back(key);
		else whiteIDs.push_back(key);
		kbMetas.emplace_back(0, false, black);
	}

	std::vector<uint8_t> keyIDs;
	keyIDs.reserve(128);
	keyIDs.insert(keyIDs.end(), whiteIDs.begin(), whiteIDs.end());
	keyIDs.insert(keyIDs.end(), blackIDs.begin(), blackIDs.end());

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

	// GLint loc = glGetAttribLocation(keyboardProgram->GetID(), "aPos");
	// std::cout << "Attribute 'a_position' is at location: " << loc << std::endl;

	keyboardProgram = std::move(kbProgram);
	keyboardVBO = std::move(kbVbo);
	keyboardVAO = std::move(vao);
	keyboardIBO = std::move(kbInstance);
	keyboardEBO = std::move(kbEbo);
	keyMetas = std::move(kbMetas);
	kbIDs = std::move(keyIDs);

	keyboardBackground = std::make_unique<Quad>();
	auto bgColor = pack->GetKeyboardInfo()->background;
	keyboardBackground->SetColor(glm::vec3(bgColor.x, bgColor.y, bgColor.z));

	CalcKeyPosAndWidth();
	UpdateKeyboardInstance();
	initialized = true;
}

void MIDIRenderer::Render()
{
	if (!initialized) return;

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
		// keyboardIBO->SetData(keyboardData, GL_DYNAMIC_DRAW);
		
#ifdef A
		CheckGLError("Keyboard Data update");

		GLint currentVAO = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
		std::cout << "Currently bound VAO: " << currentVAO << " Expected: " << keyboardVAO->GetID() << std::endl;
#endif

		glDrawElementsInstanced(
			GL_TRIANGLES,
			6,
			GL_UNSIGNED_INT,
			nullptr,
			128
		);
	}
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
	for (int i = 0; i < 128; i++)
	{
		keyPos[i] = pos / (float)width;
		pos += keyPosDiff[i % 12] * noteWidth;
	}
	int lastIdxWhite = -1;
	for (int j = 0; j < 128; j++)
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
		whiteKeyGap = std::max(1.0f, std::floor((unscaledWhiteKeyGap * widthScale)));
	}
	else
	{
		whiteKeyGap = 0.0f;
	}

	float unscaledNoteBorderWidth = pack->GetNoteInfo()->borderWidth;
	if (unscaledNoteBorderWidth > 0.0f)
	{
		// TODO: Note border width
	}
}

void MIDIRenderer::UpdateKeyboardInstance()
{
	if (!keyboardIBO)
	{
		std::cout << "Instance buffer doesn't exist yet" << std::endl;
		return;
	}

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
}

void MIDIRenderer::OnResize(int width, int height)
{
	this->width = width;
	this->height = height;
	CalcKeyPosAndWidth();
	UpdateKeyboardInstance();
}