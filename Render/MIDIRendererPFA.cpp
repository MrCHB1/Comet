#include "MIDIRendererPFA.h"
#include "Utils.h"
#include "App/MIDIApp.h"

#define NUM_WHITE_KEYS 75

void RenderColor::SetColor(uint32_t color, float dark, float veryDark)
{
	origBgr = color;
	primary = color;

	uint32_t a = (color >> 24u) & 0xFFu;
	uint32_t r = (color >> 16u) & 0xFFu;
	uint32_t g = (color >> 8u) & 0xFFu;
	uint32_t b = (color >> 0u) & 0xFFu;

	uint32_t dr = std::min(static_cast<uint32_t>(r * dark), 255u);
	uint32_t dg = std::min(static_cast<uint32_t>(g * dark), 255u);
	uint32_t db = std::min(static_cast<uint32_t>(b * dark), 255u);

	uint32_t vdr = std::min(static_cast<uint32_t>(r * veryDark), 255u);
	uint32_t vdg = std::min(static_cast<uint32_t>(g * veryDark), 255u);
	uint32_t vdb = std::min(static_cast<uint32_t>(b * veryDark), 255u);

	this->dark = (a << 24) | (dr << 16) | (dg << 8) | db;
	this->veryDark = (a << 24) | (vdr << 16) | (vdg << 8) | vdb;
}

static const float SharpRatio = 0.65f;
static const float KBPercent = 0.25f;
static const float KeyRatio = 0.1775f;

static const char* rectVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in uint aColor;

out vec3 color;

uniform float renderWidth;
uniform float renderHeight;

void main()
{
	vec3 vColor = vec3(
		float((aColor & 0xFF0000u) >> 16u) / 255.0,
		float((aColor & 0xFF00u) >> 8u) / 255.0,
		float(aColor & 0xFFu) / 255.0
	);

	vec2 vPos = vec2(aPos.x / renderWidth, 1.0 - aPos.y / renderHeight);
	gl_Position = vec4(vPos * 2.0 - 1.0, 0.0, 1.0);
	color = vColor;
})";

static const char* rectFrag = R"(#version 330 core
in vec3 color;

out vec4 fragColor;

void main()
{
	fragColor = vec4(color, 1.0);
})";

static const char* notesVert = R"(#version 330 core
layout (location = 0) in vec2 aPos; // normalized 0-1
layout (location = 1) in float nX; // screen pixel number, not normalized
layout (location = 2) in float nY;
layout (location = 3) in float nCx;
layout (location = 4) in float nCy;
layout (location = 5) in uint nMeta;

out vec2 position;
out vec2 uv;
out vec3 color;
out vec4 edges;

uniform float renderWidth;
uniform float renderHeight;

void main()
{
	vec3 vColor = vec3(
		float((nMeta & 0xFF0000u) >> 16u) / 255.0,
		float((nMeta & 0xFF00u) >> 8u) / 255.0,
		float(nMeta & 0xFFu) / 255.0
	);
	color = vColor;

	vec2 vPos = aPos;

	vPos.x = vPos.x * nCx + nX;
	vPos.y = vPos.y * nCy + nY;
	position = vPos;
	vPos = vec2(vPos.x / renderWidth, 1.0 - (vPos.y / renderHeight));

	gl_Position = vec4(vPos * 2.0 - 1.0, 0.0, 1.0);
	edges = vec4(nX, nY, nX + nCx, nY + nCy);
	uv = aPos;
})";

static const char* notesFrag = R"(#version 330 core
in vec2 uv;
in vec2 position;
in vec3 color;
in vec4 edges;

out vec4 fragColor;

uniform float deflate;

void main()
{
	vec3 noteColor = mix(color, color * 0.5, uv.x);
	noteColor = (abs(position.x - edges.x) <= deflate ||
				 abs(position.x - edges.z) <= deflate ||
				 abs(position.y - edges.y) <= deflate ||
				 abs(position.y - edges.w) <= deflate) ? color * 0.2 : noteColor;
	fragColor = vec4(noteColor, 1.0);
})";

void MIDIRendererPFA::PushRect(float x, float y, float cx, float cy, uint32_t color)
{
	PushRect(x, y, cx, cy, color, color, color, color);
}

void MIDIRendererPFA::PushRect(float x, float y, float cx, float cy, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4)
{
	immediateRects.insert(immediateRects.end(), {
		{ x, y, c1 },
		{ x + cx, y, c2 },
		{ x + cx, y + cy, c3 },
		{ x, y + cy, c4 }
	});
}

void MIDIRendererPFA::PushSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t color)
{
	PushSkew(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);
}

void MIDIRendererPFA::PushSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4)
{
	immediateRects.insert(immediateRects.end(), {
		{ x1, y1, c1 },
		{ x2, y2, c2 },
		{ x3, y3, c3 },
		{ x4, y4, c4 },
	});
}

void MIDIRendererPFA::Initialize()
{
	AbstractMIDIRenderer::Initialize();

	#pragma region Rect data
	rectProgram = ShaderProgram::Create(rectVert, rectFrag);
	rectVAO = std::make_unique<VertexArray>();
	rectVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	rectIBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	std::vector<uint32_t> indices(RECTS_PER_PASS * 6);
	for (size_t i = 0; i < RECTS_PER_PASS; i++)
	{
		indices[i * 6 + 0] = i * 4 + 0;
		indices[i * 6 + 1] = i * 4 + 1;
		indices[i * 6 + 2] = i * 4 + 2;
		indices[i * 6 + 3] = i * 4 + 2;
		indices[i * 6 + 4] = i * 4 + 3;
		indices[i * 6 + 5] = i * 4 + 0;
	}

	{
		VertexArrayBind vaoBind(*rectVAO);
		
		rectVBO->Bind();
		glBufferData(GL_ARRAY_BUFFER, RECTS_PER_PASS * 4 * sizeof(RectVertex), nullptr, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex), (void*)offsetof(RectVertex, x));

		glEnableVertexAttribArray(1);
		glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(RectVertex), (void*)offsetof(RectVertex, color));
		
		rectIBO->Bind();
		rectIBO->SetData(indices, GL_STATIC_DRAW);
	}
	#pragma endregion

	#pragma region Note data
	notesProgram = ShaderProgram::Create(notesVert, notesFrag);
	notesVAO = std::make_unique<VertexArray>();
	notesVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*notesVAO);

		std::array<float, 8> quadVerts{
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f
		};
		std::array<int, 6> quadIndices{
			0, 1, 3,
			1, 2, 3
		};
		notesVBO->Bind();
		notesVBO->SetData(quadVerts, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

		notesEBO->Bind();
		notesEBO->SetData(quadIndices, GL_STATIC_DRAW);

		notesIBO->Bind();
		notesIBO->SetData(renderNotes, GL_DYNAMIC_DRAW);

		notesVAO->SetFloatAttribute(1, 1, sizeof(RenderPFANote), offsetof(RenderPFANote, x));
		notesVAO->SetFloatAttribute(2, 1, sizeof(RenderPFANote), offsetof(RenderPFANote, y));
		notesVAO->SetFloatAttribute(3, 1, sizeof(RenderPFANote), offsetof(RenderPFANote, cx));
		notesVAO->SetFloatAttribute(4, 1, sizeof(RenderPFANote), offsetof(RenderPFANote, cy));
		notesVAO->SetIntAttribute(5, 1, sizeof(RenderPFANote), offsetof(RenderPFANote, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
	}
	#pragma endregion

	// initialize the colors
	MIDIPlayerConfig* config = app->GetConfig();
	kbBackground.SetColor(0x00999999, 0.4, 0.0);
	SetBarColor(config->render.GetBarColor());
	kbWhite.SetColor(0x00FFFFFF, 0.8, 0.6);
	kbSharp.SetColor(0x00404040, 0.5, 0.0);

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
}

void MIDIRendererPFA::LoadSequence(std::shared_ptr<MIDISequence> sequence)
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

void MIDIRendererPFA::UpdateGlobals()
{
	notesX = 0.0f;
	notesCX = static_cast<float>(width);
	notesY = 0.0f;

	allWhiteKeys = NUM_WHITE_KEYS;
	float fBuffer = 0.0f;
	whiteCX = notesCX / (allWhiteKeys + fBuffer);

	float maxKeyCY = static_cast<float>(height) * KBPercent;
	float idealKeyCY = whiteCX / KeyRatio;
	idealKeyCY = (idealKeyCY / 0.95f + 2.0f) / 0.93f;
	notesCY = std::floor(height - std::min(idealKeyCY, maxKeyCY) + 0.5f);

	GenNoteXTable();
}

void MIDIRendererPFA::GenNoteXTable()
{
	if (!noteXTableDirty) return;

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		int whiteKeys = GetNumWhiteKeys(0, i);
		float startX = (IsSharp(0) - IsSharp(i)) * SharpRatio / 2.0f;
		if (IsSharp(i))
		{
			int key12 = i % 12;
			if (key12 == 1 || key12 == 6) startX -= SharpRatio / 5.0f;
			else if (key12 == 10 || key12 == 3) startX += SharpRatio / 5.0f;
		}
		noteXTable[i] = notesX + whiteCX * (whiteKeys + startX);
	}

	noteXTableDirty = false;
}

void MIDIRendererPFA::Render(double deltaTime)
{
	UpdateGlobals();
	sceneFramebuffer->Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	if (settings.showLines) RenderLines();
	RenderImmediateRects();

	RenderNotes();

	RenderKeyboard();
	RenderImmediateRects();
	
	sceneFramebuffer->Unbind();
}

void MIDIRendererPFA::RenderLines()
{
	MIDIPlayerConfig* config = app->GetConfig();
	ImVec4 bgColor = config->render.GetBackground();

	uint32_t bgColor32 = (((uint32_t)(bgColor.x * 255.0)) << 16u) |
		(((uint32_t)(bgColor.y * 255.0)) << 8u) |
		(((uint32_t)(bgColor.z * 255.0)) << 0u);

	backgroundColor.SetColor(bgColor32, 0.7, 1.3);

	// PushRect(notesX, notesY, notesCX, notesCY, backgroundColor.primary);
	// vertical lines
	for (int i = 1; i < MIDI_KEYS; i++)
	{
		if (IsSharp(i - 1) || IsSharp(i)) continue;

		int whiteKeys = GetNumWhiteKeys(0, i);
		float startX = IsSharp(0) * SharpRatio / 2.0f;
		float x = notesX + whiteCX * (whiteKeys + startX);
		x = std::floor(x + 0.5f);
		PushRect(x - 1.0f, notesY, 3.0f, notesCY,
			backgroundColor.dark, backgroundColor.veryDark, backgroundColor.veryDark, backgroundColor.dark);
	}
	
	// "Horizontal (Hard!)" said who? ts is easy
	if (!seq) return;
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
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	std::vector<TimeSignatureEvent>& timeSigEvents = seq->timeSignatures;
	if (timeSigEvents.empty()) return;

	long measureTime = 0;
	size_t measureID = 0;

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

		if (futureTimeSignature)
		{
			// Convert the future time signature tick from 10us units to MIDI Ticks if timeline is time-based
			long futureTimeSigTick = isTimeBased
				? tempoMap->SecsToTicksFromMap(seq->resolution, (double)futureTimeSignature->tick * invTimeMultiplier)
				: futureTimeSignature->tick;

			while (measureTime < futureTimeSigTick)
			{
				const double pos = measurePos(measureTime);

				if (beat == 0 && pos > currentTime && pos < renderEnd)
				{
					float lineY = notesY + notesCY * (float)(1.0f - (pos - currentTime) * invViewRegion);
					lineY = std::floor(lineY + 0.5f);

					PushRect(notesX, lineY - 1.0f, notesCX, 3.0f,
						backgroundColor.dark, backgroundColor.dark, backgroundColor.veryDark, backgroundColor.veryDark);
				}

				measureTime += measureInc;
				beat++;
				if (beat >= timeSignature->numerator) beat = 0;
			}
		}
		else
		{
			while (measurePos(measureTime) < renderEnd)
			{
				const double pos = measurePos(measureTime);

				if (beat == 0 && pos > currentTime)
				{
					float lineY = notesY + notesCY * (float)(1.0f - (pos - currentTime) * invViewRegion);
					lineY = std::floor(lineY + 0.5f);

					PushRect(notesX, lineY - 1.0f, notesCX, 3.0f,
						backgroundColor.dark, backgroundColor.dark, backgroundColor.veryDark, backgroundColor.veryDark);
				}

				measureTime += measureInc;
				beat++;
				if (beat >= timeSignature->numerator) beat = 0;
			}
		}
	}
}

void MIDIRendererPFA::RenderKeyboard()
{
	float keysY = notesY + notesCY;
	float keysCY = height - notesCY;

	float transitionPct = 0.02f;
	float transitionCY = std::max(3.0f, std::floor(keysCY * transitionPct + 0.5f));
	float redPct = 0.05f;
	float redCY = std::floor(keysCY * redPct + 0.5f);
	float spacerCY = 2.0f;
	float topCY = std::floor((keysCY - spacerCY - redCY - transitionCY) * 0.95f + 0.5f);
	float nearCY = keysCY - spacerCY - redCY - transitionCY - topCY;

	// draw the """background""" (we call it a bar here)
	#pragma region Top bar
	PushRect(notesX, keysY, notesCX, keysCY, kbBackground.veryDark);
	PushRect(notesX, keysY, notesCX, transitionCY,
		backgroundColor.primary, backgroundColor.primary, kbBackground.veryDark, kbBackground.veryDark);
	PushRect(notesX, keysY + transitionCY, notesCX, redCY,
		kbRed.dark, kbRed.dark, kbRed.primary, kbRed.primary);
	PushRect(notesX, keysY + transitionCY + redCY, notesCX, spacerCY, kbBackground.dark);
	#pragma endregion

	float keyGap = std::max(1.0f, std::floor(whiteCX * 0.05f + 0.5f));
	float keyGap1 = keyGap - std::floor(keyGap / 2.0f + 0.5f);

	float startX = 0.0f;
	float sharpCY = topCY * 0.67f;

	// draw the white keys
	#pragma region White Keys
	float curX = notesX + startX;
	float curY = keysY + transitionCY + redCY + spacerCY;
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		if (IsSharp(i)) continue;

		auto& keyState = keyStates[i];
		if (keyState.pressed)
		{
			auto& keyColor = keyState.color;
			PushRect(curX + keyGap1, curY, whiteCX - keyGap, topCY + nearCY - 2.0f,
				keyColor.dark, keyColor.dark, keyColor.primary, keyColor.primary);
			PushRect(curX + keyGap1, curY + topCY + nearCY - 2.0f, whiteCX - keyGap, 2.0f, keyColor.dark);

			if (i == 60 && settings.showMiddleCSquare)
			{
				float mxGap = std::floor(whiteCX * 0.25f + 0.5f);
				float mcx = whiteCX - mxGap * 2.0f - keyGap;
				float my = std::max(curY + topCY + nearCY - mcx - 7.0f, curY + sharpCY + 5.0f);
				PushRect(curX + keyGap1 + mxGap, my, mcx, curY + topCY + nearCY - 7.0f - my, keyColor.dark);
			}
		}
		else
		{
			PushRect(curX + keyGap1, curY, whiteCX - keyGap, topCY + nearCY,
				kbWhite.dark, kbWhite.dark, kbWhite.primary, kbWhite.primary);
			PushRect(curX + keyGap1, curY + topCY, whiteCX - keyGap, nearCY,
				kbWhite.dark, kbWhite.dark, kbWhite.veryDark, kbWhite.veryDark);
			PushRect(curX + keyGap1, curY + topCY, whiteCX - keyGap, 2.0f,
				kbBackground.dark, kbBackground.dark, kbWhite.veryDark, kbWhite.veryDark);

			if (i == 60 && settings.showMiddleCSquare)
			{
				float mxGap = std::floor(whiteCX * 0.25f + 0.5f);
				float mcx = whiteCX - mxGap * 2.0f - keyGap;
				float my = std::max(curY + topCY - mcx - 5.0f, curY + sharpCY + 5.0f);
				PushRect(curX + keyGap1 + mxGap, my, mcx, curY + topCY - 5.0f - my, kbWhite.dark);
			}
		}

		PushRect(std::floor(curX + keyGap1 + whiteCX - keyGap + 0.5f), curY, keyGap, topCY + nearCY,
			kbBackground.veryDark, kbBackground.primary, kbBackground.primary, kbBackground.veryDark);

		curX += whiteCX;
	}
	#pragma endregion

	startX = 0.0f;
	float sharpTop = SharpRatio * 0.7f;
	curX = notesX + startX;
	curY = keysY + transitionCY + redCY + spacerCY;
	#pragma region Sharp Keys
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		if (!IsSharp(i))
		{
			curX += whiteCX;
			continue;
		}

		float nudgeX = 0.0;
		size_t key12 = i % 12;
		if (key12 == 1 || key12 == 6) nudgeX = -SharpRatio / 5.0f;
		else if (key12 == 10 || key12 == 3) nudgeX = SharpRatio / 5.0f;

		const float cx = whiteCX * SharpRatio;
		const float x = curX - whiteCX * (SharpRatio / 2.0f - nudgeX);
		const float sharpTopX1 = x + whiteCX * (SharpRatio - sharpTop) / 2.0f;
		const float sharpTopX2 = sharpTopX1 + whiteCX * sharpTop;
		
		auto& keyState = keyStates[i];
		if (keyState.pressed)
		{
			auto& keyColor = keyState.color;
			const float newNear = nearCY * 0.25f;

			PushSkew(sharpTopX1, curY + sharpCY - newNear,
					 sharpTopX2, curY + sharpCY - newNear,
					 x + cx, curY + sharpCY, x, curY + sharpCY,
					 keyColor.primary, keyColor.primary, keyColor.dark, keyColor.dark);

			PushSkew(sharpTopX1, curY - newNear,
				     sharpTopX1, curY + sharpCY - newNear,
				     x, curY + sharpCY, x, curY,
				     keyColor.primary, keyColor.primary, keyColor.dark, keyColor.dark);

			PushSkew(sharpTopX2, curY + sharpCY - newNear,
				     sharpTopX2, curY - newNear,
				     x + cx, curY, x + cx, curY + sharpCY,
				     keyColor.primary, keyColor.primary, keyColor.dark, keyColor.dark);

			PushRect(sharpTopX1, curY - newNear, sharpTopX2 - sharpTopX1, sharpCY, keyColor.dark);

			PushSkew(sharpTopX1, curY - newNear,
				     sharpTopX2, curY - newNear,
				     sharpTopX2, curY - newNear + sharpCY * 0.35f,
				     sharpTopX1, curY - newNear + sharpCY * 0.25f,
				     keyColor.primary, keyColor.primary, keyColor.primary, keyColor.primary);

			PushSkew(sharpTopX1, curY - newNear + sharpCY * 0.25f,
					 sharpTopX2, curY - newNear + sharpCY * 0.35f,
					 sharpTopX2, curY - newNear + sharpCY * 0.75f,
					 sharpTopX1, curY - newNear + sharpCY * 0.65f,
					 keyColor.primary, keyColor.primary, keyColor.dark, keyColor.dark);
		}
		else
		{
			PushSkew(sharpTopX1, curY + sharpCY - nearCY,
				sharpTopX2, curY + sharpCY - nearCY,
				x + cx, curY + sharpCY, x, curY + sharpCY,
				kbSharp.primary, kbSharp.primary, kbSharp.veryDark, kbSharp.veryDark);

			PushSkew(sharpTopX1, curY - nearCY,
				sharpTopX1, curY + sharpCY - nearCY,
				x, curY + sharpCY, x, curY,
				kbSharp.primary, kbSharp.primary, kbSharp.veryDark, kbSharp.veryDark);

			PushSkew(sharpTopX2, curY + sharpCY - nearCY,
				sharpTopX2, curY - nearCY,
				x + cx, curY, x + cx, curY + sharpCY,
				kbSharp.primary, kbSharp.primary, kbSharp.veryDark, kbSharp.veryDark);

			PushRect(sharpTopX1, curY - nearCY,
				sharpTopX2 - sharpTopX1, sharpCY,
				kbSharp.veryDark);

			PushSkew(sharpTopX1, curY - nearCY,
				sharpTopX2, curY - nearCY,
				sharpTopX2, curY - nearCY + sharpCY * 0.45f,
				sharpTopX1, curY - nearCY + sharpCY * 0.35f,
				kbSharp.dark, kbSharp.dark, kbSharp.primary, kbSharp.primary);

			PushSkew(sharpTopX1, curY - nearCY + sharpCY * 0.35f,
				sharpTopX2, curY - nearCY + sharpCY * 0.45f,
				sharpTopX2, curY - nearCY + sharpCY * 0.65f,
				sharpTopX1, curY - nearCY + sharpCY * 0.55f,
				kbSharp.primary, kbSharp.primary, kbSharp.veryDark, kbSharp.veryDark);
		}
	}
	#pragma endregion

	// after rendering the keyboard, reset state so they don't bleed into next frame
	for (auto& keyState : keyStates)
	{
		keyState.pressed = false;
	}
}

void MIDIRendererPFA::RenderImmediateRects()
{
	if (immediateRects.empty()) return;

	ShaderBind shader(*rectProgram);
	VertexArrayBind vaoBind(*rectVAO);
	rectVBO->Bind();

	rectProgram->SetFloat("renderWidth", static_cast<float>(width));
	rectProgram->SetFloat("renderHeight", static_cast<float>(height));

	size_t totalRects = immediateRects.size() / 4;
	size_t rectsDrawn = 0;

	while (rectsDrawn < totalRects)
	{
		size_t rectsToDraw = std::min((size_t)RECTS_PER_PASS, totalRects - rectsDrawn);

		size_t verticesToDraw = rectsToDraw * 4;
		size_t vertexOffset = rectsDrawn * 4;

		glBufferSubData(GL_ARRAY_BUFFER, 0, verticesToDraw * sizeof(RectVertex), immediateRects.data() + vertexOffset);
		glDrawElements(GL_TRIANGLES, rectsToDraw * 6, GL_UNSIGNED_INT, nullptr);
		rectsDrawn += rectsToDraw;
	}

	immediateRects.clear();
}

void MIDIRendererPFA::RenderNotes()
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

	ShaderBind shader(*notesProgram);
	notesProgram->SetFloat("deflate", std::clamp(std::round(whiteCX * 0.15f / 2.0f), 1.0f, 3.0f));
	notesProgram->SetFloat("renderWidth", width);
	notesProgram->SetFloat("renderHeight", height);

	VertexArrayBind vaoBind(*notesVAO);
	BufferBind vboBind(*notesVBO);
	BufferBind eboBind(*notesEBO);
	BufferBind iboBind(*notesIBO);

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

		bool isBlack = IsSharp(id);

		float cx = whiteCX;
		float x = noteXTable[id];

		if (isBlack)
		{
			cx *= SharpRatio;
		}

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
				keyStates[id].color.SetColor(colors.GetColor(note->track, note->channel));
				keyStates[id].pressed = true;

				notesPassed++;
				polyphony++;
			}

			float yDistStart = (float)((noteStart - accTime) * invViewRegion) * notesCY;
			float yDistEnd = (float)((noteEnd - accTime) * invViewRegion) * notesCY;

			float noteBottom = notesY + notesCY - yDistStart;
			float noteTop = notesY + notesCY - yDistEnd;

			float noteCY = noteBottom - noteTop;
			if (settings.roundedNoteLengths)
			{
				noteTop = std::round(noteTop);
				noteCY = std::round(noteCY);
			}

			renderNotes[noteID++] = {
				x,
				noteTop,
				isBlack ? cx : whiteCX,     
				noteCY, 
				colors.GetColor(note->track, note->channel)
			};

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

void MIDIRendererPFA::UploadNoteBuffer(size_t count)
{
	if (!notesIBO) return;

	notesIBO->Bind();

	glBufferSubData(GL_ARRAY_BUFFER,
		0,
		count * sizeof(RenderPFANote),
		renderNotes.data());

	glDrawElementsInstanced(
		GL_TRIANGLES,
		6,
		GL_UNSIGNED_INT,
		nullptr,
		count
	);
}

void MIDIRendererPFA::RenderSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();
	ImGui::Text("Show Middle C Square");
	ImGui::SameLine();
	ImGui::Checkbox("##middleCSquare", &settings.showMiddleCSquare);

	ImGui::Text("Show lines");
	ImGui::SameLine();
	ImGui::Checkbox("##showLines", &settings.showLines);

	ImGui::Text("Rounded note lengths");
	ImGui::SetItemTooltip("In the original PFA renderer, notes that were visually too short were invisible due to rounded note lengths. This option replicates that behaviour.");
	ImGui::SameLine();
	ImGui::Checkbox("##roundedLengths", &settings.roundedNoteLengths);

	ImGui::Text("Bar color");
	ImGui::SameLine();
	ImVec4 barColor = config->render.GetBarColor();
	float barColorV[3]{ barColor.x, barColor.y, barColor.z };
	if (ImGui::ColorEdit3("##barColor", barColorV))
	{
		ImVec4 newBarColor(barColorV[0], barColorV[1], barColorV[2], 1.0);
		SetBarColor(newBarColor);
		config->render.SetBarColor(barColorV[0], barColorV[1], barColorV[2]);
	}
	AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererPFA::OnResize(int width, int height)
{
	AbstractMIDIRenderer::OnResize(width, height);
	noteXTableDirty = true;
}

void MIDIRendererPFA::SetBarColor(ImVec4 color)
{
	uint32_t color32 = (((uint32_t)(color.x * 255.0)) << 16u) |
		(((uint32_t)(color.y * 255.0)) << 8u) |
		(((uint32_t)(color.z * 255.0)) << 0u);
	SetBarColor(color32);
}

void MIDIRendererPFA::SetBarColor(uint32_t color)
{
	kbRed.SetColor(color & 0xFFFFFF, 0.5);
}