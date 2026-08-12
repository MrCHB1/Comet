#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include "RenderEngine/TextureArray.h"
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <random>

#define RECT_BUFFER_SIZE 32768
#define NOTE_BUFFER_SIZE 131072
#define NOTES_MAX_BATCHES 131072
#define IS_BLACK(n) (((n) % 12) == 1 || ((n) % 12) == 3 || \
	((n) % 12) == 6 || ((n) % 12) == 8 || ((n) % 12) == 10)

#define RECT_TEXTURE_FLAG 0x80000000u
#define RECT_DATA_MASK 0x7FFFFFFFu

#define NUM_TEXTURES 13
#define TEXTURE_ARRAY_SLOT 0

enum SynthesiaStyle
{
	SYNTHESIA_10,
	SYNTHESIA_9
};

struct SynthesiaRenderSettings
{
	bool useNativeNoteColors = true;
	bool showOutOfBoundNotes = true;
	bool renderKeySparkle = true;
	bool renderBackground = true;
	bool showKeyOctaves = true;

	SynthesiaStyle style = SYNTHESIA_10;

	static SynthesiaRenderSettings Default()
	{
		return SynthesiaRenderSettings{};
	}
};

#pragma region Render structs
#pragma pack(push, 1)
struct TexturedRectInstance
{
	glm::vec2 position;
	glm::vec2 size;

	float rotation;

	glm::vec2 uv0;
	glm::vec2 uv1;

	int textureIndex;

	// last bit serves as a flag for either being a texture or colored.
	// as a consequence we lose 2x the amount of alpha values but it should probably make little difference
	uint32_t meta;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RenderNoteSynthesia
{
	float x, y;
	float width, height;
	uint32_t meta;
};
#pragma pack(pop)

struct KeyboardState
{
	uint32_t color = 0x000000;
	bool pressed = false;
};
#pragma endregion

#pragma region Particle classes

class Particle
{
public:
	float life = 0.0f;

	virtual void Step(double delta) = 0;
};

class KeyHazeParticle : public Particle
{
public:
	uint8_t key;
	float brightness = 0.0f;
	glm::vec2 pos;

	KeyHazeParticle(uint8_t key, std::mt19937& random);
	void Step(double delta) override;
protected:
	static float maxLife;
};

class KeySparkParticle : public Particle
{
public:
	uint8_t key;
	float brightness = 0.0f;
	glm::vec2 pos;
	float rotation;
	float size = 0.0f;
	bool flipped;

	KeySparkParticle(uint8_t key, std::mt19937& random);
	void Step(double delta) override;
protected:
	static float maxLife;
	static float minSize;
};

class KeyDebrisParticle : public Particle
{
public:
	glm::vec2 pos;
	glm::vec2 vel;

	float rotation;
	float size;

	uint8_t key;

	KeyDebrisParticle(uint8_t key, std::mt19937& random);
	void Step(double delta) override;
};

#pragma endregion

// Index of each source image within the shared texture array. Order must
// match the LoadLayer() calls in Initialize().
enum TextureLayer : int
{
	LAYER_BAR = 0,
	LAYER_KEY_BLACK,
	LAYER_KEY_BLACK_PRESSED,
	LAYER_KEY_WHITE,
	LAYER_KEY_WHITE_PRESSED,
	LAYER_KEY_WHITE_WHOLE,
	LAYER_KEY_WHITE_WHOLE_PRESSED,
	LAYER_SHADOW_LARGE,
	LAYER_SHADOW_UNPRESSED,
	LAYER_SHADOW_PRESSED,

	// ---- PARTICLE TEXTURES ----

	LAYER_PARTICLE_DEBRIS,
	LAYER_PARTICLE_SPARKLE,
	LAYER_PARTICLE_HAZE,
	LAYER_COUNT // must equal NUM_TEXTURES
};

static_assert(LAYER_COUNT == NUM_TEXTURES, "TextureLayer enum must match NUM_TEXTURES");

class MIDIRendererSynthesia : public AbstractMIDIRenderer
{
public:
	MIDIRendererSynthesia(MIDIApp* app) : AbstractMIDIRenderer(app) {}
	void Initialize() override;
	void LoadSequence(std::shared_ptr<MIDISequence> sequence) override;
	void Render(double deltaTime) override;
	void RenderSettings() override;
	void ResetSettings() override;
	void OnResize(int width, int height) override;

	std::string GetSerializationKey() const override { return "synthesia"; }
	YAML::Node GetSettings() override;
	void LoadSettings(const YAML::Node& node) override;

	void ResetRenderer() override;
private:
#pragma region Textures
	// All keyboard/note/shadow textures live as layers in one array so the
	// rect shader can sample any of them via a single sampler2DArray.
	std::unique_ptr<TextureArray> textures;
	// Per-layer UV rect (u0,v0,u1,v1) mapping to that layer's real image
	// content, since layers share one canvas size but source images differ.
	std::array<glm::vec4, NUM_TEXTURES> layerUV{};
#pragma endregion

#pragma region Rect stuff
	std::unique_ptr<ShaderProgram> rectProgram;
	std::unique_ptr<VertexArray> rectVAO;
	std::unique_ptr<Buffer> rectVBO;
	std::unique_ptr<Buffer> rectIBO;
	std::unique_ptr<Buffer> rectEBO;

	std::array<TexturedRectInstance, RECT_BUFFER_SIZE> renderRects{};
	size_t rectDrawCount = 0;
#pragma endregion

#pragma region Notes
	std::unique_ptr<ShaderProgram> notesProgram;
	std::unique_ptr<VertexArray> notesVAO;
	std::unique_ptr<Buffer> notesVBO;
	std::unique_ptr<Buffer> notesIBO;
	std::unique_ptr<Buffer> notesEBO;

	std::array<RenderNoteSynthesia, NOTE_BUFFER_SIZE> renderNotes{};
#pragma endregion

#pragma region Note textures
	std::unique_ptr<GPUImage> noteWhiteBody;
	std::unique_ptr<GPUImage> noteWhiteTop;
	std::unique_ptr<GPUImage> noteWhiteBottom;
	std::unique_ptr<GPUImage> noteBlackBody;
	std::unique_ptr<GPUImage> noteBlackTop;
	std::unique_ptr<GPUImage> noteBlackBottom;
	std::unique_ptr<GPUImage> noteOOB;
#pragma endregion

#pragma region Particle variables
	std::vector<std::vector<std::unique_ptr<Particle>>*> fullParticlesArray{};
	std::array<std::vector<std::unique_ptr<Particle>>, MIDI_KEYS> keyHazeParticles{};
	std::array<std::vector<std::unique_ptr<Particle>>, MIDI_KEYS> keySparkParticles{};
	std::vector<std::unique_ptr<Particle>> keyDebrisParticles{};
#pragma endregion

#pragma region Variables
	float initialKeyboardHeight = 0.158;
	float initialBarHeight = 0.0045;

	float keyboardHeight = 0.158;

	float blackKeyWidth = 0.0f;
	float whiteKeyWidth = 0.0f;

	std::array<float, MIDI_KEYS> keyPos{};
	std::array<float, MIDI_KEYS> keyWidth{};
	std::array<size_t, MIDI_KEYS> keyNum{};
	std::array<KeyboardState, MIDI_KEYS> keyStates{};
	std::array<size_t, MIDI_KEYS> kbIDs{};

	std::array<size_t, MIDI_KEYS> startRenderIDs;
	std::array<size_t, MIDI_KEYS> endRenderIDs;
	long lastTime = -1;

	bool keyArrayDirty = true;
	bool initialized = false;

	SynthesiaRenderSettings renderSettings;
	std::mt19937 random{ std::random_device{}() };
#pragma endregion

	void InitializeTextures();
	void InitializeParticleSystems();
	void CalculateKeyboardData();
	void GenerateKeyLayoutArrays();
	void UpdateRenderer();

	void UpdateStyle();
	void RenderStyleSettings();

	void RenderBackground();
	void RenderLines();
	void RenderDividerLines();
	void RenderMeasureLines();
	void RenderKeyboard();
	void RenderOctaveTextOverlays();

	void GenerateParticles(double deltaTime);
	void RenderParticles();
	void UpdateParticles(double deltaTime);

	void RenderNotes();
	void RenderOutOfBoundNotes();
	void FlushNotes(size_t count);

	void PushQuad(float x, float y, float width, float height, uint32_t color)
	{
		PushRotatedQuad(x, y, width, height, 0.0f, color);
	}

	void PushRotatedQuad(float x, float y, float width, float height, float rotation, uint32_t color)
	{
		if (rectDrawCount >= RECT_BUFFER_SIZE)
		{
			RenderQuads(RECT_BUFFER_SIZE);
			rectDrawCount = 0;
		}

		renderRects[rectDrawCount++] = { glm::vec2(x, y), glm::vec2(width, height), rotation, glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 0.0f), 0, color & RECT_DATA_MASK };
	}

	void PushQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, int layer, uint32_t color = 0xFFFFFFFF)
	{
		PushRotatedQuad(x, y, width, height, 0.0f, u0, v0, u1, v1, layer, color);
	}

	void PushRotatedQuad(float x, float y, float width, float height, float rotation, float u0, float v0, float u1, float v1, int layer, uint32_t color = 0xFFFFFFFF)
	{
		if (rectDrawCount >= RECT_BUFFER_SIZE)
		{
			RenderQuads(RECT_BUFFER_SIZE);
			rectDrawCount = 0;
		}

		const glm::vec4& uv = layerUV[layer];

		glm::vec2 realUV0 = glm::vec2(uv.x + u0 * uv.z, uv.y + v0 * uv.w);
		glm::vec2 realUV1 = glm::vec2(uv.x + u1 * uv.z, uv.y + v1 * uv.w);

		renderRects[rectDrawCount++] = { glm::vec2(x, y), glm::vec2(width, height), rotation, realUV0, realUV1, layer, RECT_TEXTURE_FLAG | color };
	}

	void RenderQuads(size_t count);

	void ResetKeyboardState();
	void KillAllParticles();
};