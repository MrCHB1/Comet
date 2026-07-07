#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include <memory>
#include "RenderEngine/Buffers.h"
#include <glm/glm.hpp>
#include "MIDI/MIDIDefs.h"
#include "MIDIRenderer.h" // for RenderNote struct
#include <filesystem>

struct RenderNote;

struct MIDITrailSettings
{
	float noteDownSpeed = 48.0f;
	float noteUpSpeed = 16.0f;

	glm::vec3 cameraPos = glm::vec3(0.5f, 0.466f, 0.385f);
	glm::vec3 cameraRotation = glm::vec3(30.798f, 0.0f, 0.0f);
	float cameraFOV = 59.327f;

	float frontRenderCutoff = 18.0f;
	float backRenderCutoff = 0.2f;

	bool notes3D = true;
	float noteTransparency = 0.0f;
	bool eatNotes = false;

	bool showKeyboard = true;

	bool showMeasureLines = true;
	bool showOuterBorders = true;
	float lineTransparency = 0.5;

	bool showAura = true;
	std::filesystem::path auraTexture = "./assets/render/MIDITrail/aura/ring.png";
	float auraSize = 0.04f;

	MIDITrailSettings() = default;
};

#define NOTE_BUFFER_SIZE 65536
#define NOTES_MAX_BATCHES 128
#define MEASURE_LINE_BUFFER_SIZE 128

#pragma pack(push, 1)
struct RenderKeyboardKeyMT
{
	float left;
	float right;
	float pressFactor;
	uint32_t meta;
	RenderKeyboardKeyMT() = default;
	RenderKeyboardKeyMT(float left, float right, float pressFactor, uint32_t meta) : left(left), right(right), pressFactor(pressFactor), meta(meta) {}
};
#pragma pack(pop)

struct KeyboardMetaMT
{
	static constexpr uint32_t META_PRESSED = 1u << 24;
	static constexpr uint32_t META_BLACK = 1u << 25;
	bool pressed = false;
	bool black = false;
	uint32_t color = 0x000000;
	KeyboardMetaMT() = default;
	KeyboardMetaMT(uint32_t color, bool pressed, bool black)
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

#pragma pack(push, 1)
struct RenderMeasureLine
{
	float time;
	RenderMeasureLine() = default;
	RenderMeasureLine(float time) : time(time) {}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RenderAura
{
	float size;
	float pos;
	uint32_t meta;
	RenderAura() = default;
	RenderAura(float size, float pos, uint32_t meta) : size(size), pos(pos), meta(meta) {}
};
#pragma pack(pop)

struct AuraData
{
	float size;
	uint32_t meta;
	bool active;
};

#define KEY_IS_BLACK(n) \
( ((n) % 12) == 1 || \
  ((n) % 12) == 3 || \
  ((n) % 12) == 6 || \
  ((n) % 12) == 8 || \
  ((n) % 12) == 10 )

// ported zenith's midi trail renderer to comet. funny 3d renderer
class MIDIRendererMIDITrail : public AbstractMIDIRenderer
{
public:
	MIDIRendererMIDITrail(MIDIApp* app) : AbstractMIDIRenderer(app) {}

	void Initialize() override;
	void ResetRenderer() override;
	void LoadSequence(std::shared_ptr<MIDISequence> sequence) override;
	void Render(double deltaTime) override;
	void RenderSettings() override;
	void ResetSettings() override;
private:
	MIDITrailSettings settings{};
	#pragma region Keyboard Data
	std::unique_ptr<ShaderProgram> whiteKeyProgram;
	std::unique_ptr<ShaderProgram> blackKeyProgram;

	// has multiple VBOs for each white key, since they have different shapes
	std::unique_ptr<VertexArray> whiteKeyVAO;
	std::unique_ptr<Buffer> whiteKeyVBOs[7 * 3]{};
	std::unique_ptr<Buffer> whiteKeyEBO;
	std::unique_ptr<Buffer> whiteKeyColVBO;
	
	std::unique_ptr<VertexArray> blackKeyVAO;
	std::unique_ptr<Buffer> blackKeyVBO;
	std::unique_ptr<Buffer> blackKeyEBO;
	std::unique_ptr<Buffer> blackKeyColVBO;

	std::unique_ptr<ShaderProgram> noteProgram;

	size_t numWhiteKeys = 0;
	size_t numBlackKeys = 0;

	std::array<float, MIDI_KEYS> keyPos{};
	std::array<float, MIDI_KEYS> keyWidth{};
	std::array<RenderKeyboardKeyMT, MIDI_KEYS> keyboardData{};
	std::array<KeyboardMetaMT, MIDI_KEYS> keyMetas{};
	std::array<uint8_t, MIDI_KEYS> kbIDs{};
	#pragma endregion

	#pragma region Note data
	std::unique_ptr<ShaderProgram> notesProgram;
	std::unique_ptr<Buffer> notesVBO;
	std::unique_ptr<VertexArray> notesVAO;
	std::unique_ptr<Buffer> notesIBO;
	std::unique_ptr<Buffer> notesEBO;

	std::array<RenderNote, NOTE_BUFFER_SIZE> renderNotes{};
	std::array<size_t, MIDI_KEYS> startRenderIDs{};
	std::array<size_t, MIDI_KEYS> endRenderIDs{};

	long lastTime = 0;
	#pragma endregion

	#pragma region Separator data
	std::vector<glm::vec3> separatorVerts{};
	std::unique_ptr<ShaderProgram> separatorProgram;
	std::unique_ptr<Buffer> separatorVBO;
	std::unique_ptr<VertexArray> separatorVAO;
	#pragma endregion

	#pragma region Measure line data
	std::unique_ptr<ShaderProgram> measureProgram;
	std::unique_ptr<Buffer> measureVBO;
	std::unique_ptr<VertexArray> measureVAO;
	std::unique_ptr<Buffer> measureIBO;
	std::unique_ptr<Buffer> measureEBO;

	std::array<RenderMeasureLine, MEASURE_LINE_BUFFER_SIZE> renderMeasureLines{};
	#pragma endregion

	#pragma region Aura data
	std::unique_ptr<ShaderProgram> auraProgram;
	std::unique_ptr<Buffer> auraVBO;
	std::unique_ptr<VertexArray> auraVAO;
	std::unique_ptr<Buffer> auraIBO;
	std::unique_ptr<Buffer> auraEBO;

	std::unique_ptr<GPUImage> auraTexture;

	std::array<RenderAura, MIDI_KEYS> renderAuras{};
	std::array<AuraData, MIDI_KEYS> auraData{};
	#pragma endregion

	#pragma region Camera data
	glm::mat4 projection = glm::mat4(1.0);
	glm::mat4 view = glm::mat4(1.0);
	glm::mat4 model = glm::mat4(1.0);
	#pragma endregion

	float keyboardHeight = 0.014f;

	void CalcKeyPosAndWidth();

	void RenderNotes();
	void UploadNoteBuffer(size_t count);

	void RenderKeyboard();
	void UpdateKeyboard(double deltaTime);

	void RenderSeparatorLines();
	void UpdateSeparatorLines(float start, float end);

	void RenderMeasureLines();

	void LoadAuraTexture();
	void RenderAuras();
	void UpdateAuras(double deltaTime);

	void UpdateMatrices();
	glm::mat4 GetViewMatrixFromEuler();
};