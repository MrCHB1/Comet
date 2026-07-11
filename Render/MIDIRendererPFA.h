#pragma once

/* Mimicks how PFA(viz) does the rendering, ensuring a 1:1 replica (at least visually). */
#include "MIDI/MIDIDefs.h"
#include "Renderer/AbstractMIDIRenderer.h"
#include "RenderEngine/Buffers.h"
#include "RenderEngine/Shaders.h"

struct PFARenderSettings
{
	bool showMiddleCSquare = true;
	bool showLines = true;
	bool roundedNoteLengths = false;
};

#define RECTS_PER_PASS 16384
#define NOTE_BUFFER_SIZE 262144
#define MAX_BATCHES 64

struct RenderColor
{
	RenderColor() { SetColor(0x00000000); }
	void SetColor(uint32_t color, float dark = 0.5f, float veryDark = 0.2f);
	uint32_t primary, dark, veryDark, origBgr;
};

#pragma pack(push, 1)
struct RectVertex {
	float x;
	float y;
	uint32_t color;
};
#pragma pack(pop)

struct KeyState
{
	bool pressed = false;
	RenderColor color;
};

#pragma pack(push, 1)
struct RenderPFANote
{
	float x;
	float y;
	float cx;
	float cy;
	uint32_t meta;
};
#pragma pack(pop)

class MIDIRendererPFA : public AbstractMIDIRenderer
{
public:
	MIDIRendererPFA(MIDIApp* app) : AbstractMIDIRenderer(app) {}
	void Initialize() override;
	void LoadSequence(std::shared_ptr<MIDISequence> sequence) override;
	void Render(double deltaTime) override;
	void RenderSettings() override;
	void ResetSettings() override
	{
		settings = PFARenderSettings();
	}
	void OnResize(int width, int height) override;
private:
	std::vector<RectVertex> immediateRects;
	PFARenderSettings settings{};

	#pragma region Rect data
	std::unique_ptr<ShaderProgram> rectProgram;
	std::unique_ptr<VertexArray> rectVAO;
	std::unique_ptr<Buffer> rectVBO;
	std::unique_ptr<Buffer> rectIBO;
	#pragma endregion

	#pragma region Note data
	std::unique_ptr<ShaderProgram> notesProgram;
	std::unique_ptr<VertexArray> notesVAO;
	std::unique_ptr<Buffer> notesVBO;
	std::unique_ptr<Buffer> notesIBO;
	std::unique_ptr<Buffer> notesEBO;

	std::array<RenderPFANote, NOTE_BUFFER_SIZE> renderNotes{};
	#pragma endregion

	#pragma region PFA "Globals"
	float notesX = 0.0f;
	float notesCX = 0.0f;
	float notesY = 0.0f;
	float notesCY = 0.0f;

	size_t allWhiteKeys = 0;
	float whiteCX = 0.0f;
	#pragma endregion

	#pragma region Render Colors
	RenderColor backgroundColor;
	RenderColor kbBackground;
	RenderColor kbRed;
	RenderColor kbWhite;
	RenderColor kbSharp;
	#pragma endregion

	bool noteXTableDirty = true;
	// float noteXTable[128]{};
	std::array<float, MIDI_KEYS> noteXTable{};
	std::array<KeyState, MIDI_KEYS> keyStates{};

	double lastTime = 0.0f;
	std::array<size_t, MIDI_KEYS> startRenderIDs{};
	std::array<size_t, MIDI_KEYS> endRenderIDs{};

	std::array<uint8_t, MIDI_KEYS> kbIDs;

	void UpdateGlobals();
	void GenNoteXTable();

	void PushRect(float x, float y, float cx, float cy, uint32_t color);
	void PushRect(float x, float y, float cx, float cy, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4);
	void PushSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t color);
	void PushSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4);

	void RenderKeyboard();
	void RenderLines();
	void RenderImmediateRects();

	void RenderNotes();
	void UploadNoteBuffer(size_t count);

	void SetBarColor(ImVec4 color);
	void SetBarColor(uint32_t color);
};