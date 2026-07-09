#pragma once
#include "./Renderer/AbstractMIDIRenderer.h"
#include "../ResourcePack/ResourcePack.h"
#include "GPUImage.h"
#include "./RenderEngine/Shaders.h"
#include "./RenderEngine/Buffers.h"
#include <array>
#include "Renderer/QuadDrawer.h"
#include "NoteCounter/NoteCounterInfo.h"
#include "RenderView.h"
#include <mutex>
#include "../MIDI/MIDIDefs.h"
#include "ColorAsset.h"
#include <queue>

#define NOTE_BUFFER_SIZE 65536
#define NOTES_MAX_BATCHES 128

class MIDIApp;

#pragma region Piano Keyboard Data

#pragma pack(push, 1)
struct RenderKeyboardKey
{
	float left;
	float right;
	uint32_t meta;
	RenderKeyboardKey() = default;
	RenderKeyboardKey(float left, float right, uint32_t meta) : left(left), right(right), meta(meta) {}
};
#pragma pack(pop)

struct KeyboardMeta
{
	static constexpr uint32_t META_PRESSED = 1u << 24;
	static constexpr uint32_t META_BLACK = 1u << 25;

	bool pressed = false;
	bool black = false;
	uint32_t color = 0x000000;

	KeyboardMeta() = default;
	KeyboardMeta(uint32_t color, bool pressed, bool black)
		: color(color), pressed(pressed), black(black) {
	}

	constexpr uint32_t GetMeta() const
	{
		return (color & 0x00FFFFFF)
			| (pressed ? META_PRESSED : 0)
			| (black ? META_BLACK : 0);
	}

	void MarkPressed(bool pressed)
	{
		this->pressed = pressed;
	}

	void MarkBlack(bool black)
	{
		this->black = black;
	}
};

#pragma endregion

#pragma region Note Data

#pragma pack(push, 1)
struct RenderNote
{
	float left;
	float right;
	float start;
	float end;
	uint32_t color;
	RenderNote() = default;
	RenderNote(float left, float right, float start, float end, uint32_t color)
		: left(left), right(right), start(start), end(end), color(color) {
	}
};
#pragma pack(pop)

#pragma endregion

#define KEY_IS_BLACK(n) \
( ((n) % 12) == 1 || \
  ((n) % 12) == 3 || \
  ((n) % 12) == 6 || \
  ((n) % 12) == 8 || \
  ((n) % 12) == 10 )

const float keyPosDiff[] = {
	0.6F, 0.4F, 0.8F, 0.2F, 1.0F, 0.6F, 0.4F, 0.675F, 0.325F, 0.8F, 0.2F, 1.0F
};

class MIDIRenderer : public AbstractMIDIRenderer
{
public:
    MIDIRenderer(MIDIApp* app) : AbstractMIDIRenderer(app), keyPos{128}, keyWidth{128}
	{
		keyboardData.fill(RenderKeyboardKey());
		keyMetas.fill(KeyboardMeta());
	}
	void LoadResourcePack(std::shared_ptr<ResourcePack> pack, bool loadColors);

	void Initialize() override;
	void InitializeFromConfig();

	// Overrides from AbstractMIDIRenderer
	void LoadSequence(std::shared_ptr<MIDISequence> seq) override;
	void UnloadSequence() override;
	void Render(double deltaTime) override;
	void RenderSettings() override;
	void ResetSettings() override;
	void OnResize(int width, int height) override;
	void SetBarColor(float r, float g, float b)
	{
		if (!initialized) return;
		ShaderBind bind(*keyboardProgram);
		keyboardProgram->SetVec3("barColor", glm::vec3(r, g, b));
	}
	void SetBackgroundColor(float r, float g, float b)
	{
		if (!initialized) return;
		// keyboardBackground->SetColor(glm::vec3(r, g, b));
	}

private:
#pragma region Keyboard and note textures
	std::unique_ptr<GPUImage> textureNote;
	std::unique_ptr<GPUImage> textureNoteBlack;
	std::unique_ptr<GPUImage> textureNoteEdge;
	std::unique_ptr<GPUImage> textureKeyWhite;
	std::unique_ptr<GPUImage> textureKeyBlack;
	std::unique_ptr<GPUImage> textureKeyWhitePressed;
	std::unique_ptr<GPUImage> textureKeyBlackPressed;
	std::unique_ptr<GPUImage> textureKeyWhiteMask;
	std::unique_ptr<GPUImage> textureKeyBlackMask;
	std::unique_ptr<GPUImage> textureKeyWhiteMaskPressed;
	std::unique_ptr<GPUImage> textureKeyBlackMaskPressed;
#pragma endregion

#pragma region Keyboard
	std::unique_ptr<ShaderProgram> keyboardProgram;
	std::unique_ptr<Buffer> keyboardVBO;
	std::unique_ptr<VertexArray> keyboardVAO;
	std::unique_ptr<Buffer> keyboardIBO;
	std::unique_ptr<Buffer> keyboardEBO;

	std::array<float, MIDI_KEYS> keyPos;
	std::array<float, MIDI_KEYS> keyWidth;
	std::array<RenderKeyboardKey, MIDI_KEYS> keyboardData;
	std::array<KeyboardMeta, MIDI_KEYS> keyMetas;
	std::array<uint8_t, MIDI_KEYS> kbIDs;
#pragma endregion

#pragma region Notes
	std::unique_ptr<ShaderProgram> notesProgram;
	std::unique_ptr<Buffer> notesVBO;
	std::unique_ptr<VertexArray> notesVAO;
	std::unique_ptr<Buffer> notesIBO;
	std::unique_ptr<Buffer> notesEBO;

	std::array<RenderNote, NOTE_BUFFER_SIZE> renderNotes;
	std::array<size_t, MIDI_KEYS> startRenderIDs;
	std::array<size_t, MIDI_KEYS> endRenderIDs;
	long lastTime = -1;
#pragma endregion

	std::unique_ptr<Quad> keyboardBackground;

	bool initialized = false;
	bool keyboardDirty = false;

	float whiteKeyGap = 0.0f;
	float keyboardHeightBlack = 0.0f;
	float keyboardHeightWhite = 0.0f;
	float keyboardHeight = 0.0f;
	float noteBorderWidth = 0.0f;

	std::shared_ptr<ResourcePack> pack;

	std::mutex renderMutex;

	void CalcKeyPosAndWidth();
	void UpdateKeyboardInstance();
	void RenderNotes();
	void UploadNoteBuffer(size_t count);
	void RenderKeyboard();
	void LoadMaskTexture(ResourcePack* pack, std::unique_ptr<GPUImage>& maskTexture, const char* name);
};
