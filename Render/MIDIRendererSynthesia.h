#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include "RenderEngine/TextureArray.h"
#include <vector>
#include <array>
#include <glm/glm.hpp>

#define RECT_BUFFER_SIZE 131072
#define IS_BLACK(n) (((n) % 12) == 1 || ((n) % 12) == 3 || \
	((n) % 12) == 6 || ((n) % 12) == 8 || ((n) % 12) == 10)

#define RECT_TEXTURE_FLAG 0x80000000u
#define RECT_DATA_MASK 0x7FFFFFFFu

#define NUM_TEXTURES 10
#define TEXTURE_ARRAY_SLOT 0

#pragma pack(push, 1)
struct TexturedRectInstance
{
	glm::vec2 position;
	glm::vec2 size;

	glm::vec2 uv0;
	glm::vec2 uv1;

	int textureIndex;

	// last bit serves as a flag for either being a texture or colored.
	// as a consequence we lose 2x the amount of alpha values but it should probably make little difference
	uint32_t meta;
};
#pragma pack(pop)

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
	// LAYER_NOTE_CAP_TOP,
	// LAYER_NOTE_CAP_BOTTOM,
	// LAYER_NOTE_BODY,
	LAYER_SHADOW_LARGE,
	LAYER_SHADOW_UNPRESSED,
	LAYER_SHADOW_PRESSED,
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

#pragma region Variables
	float initialKeyboardHeight = 0.158;
	float initialBarHeight = 0.0045;

	float keyboardHeight = 0.158;

	float blackKeyWidth = 0.0f;
	float whiteKeyWidth = 0.0f;

	std::array<float, MIDI_KEYS> keyPos{};
	std::array<float, MIDI_KEYS> keyWidth{};
	std::array<size_t, MIDI_KEYS> keyNum{};

	bool keyArrayDirty = true;
#pragma endregion

	void InitializeTextures();
	void CalculateKeyboardData();
	void GenerateKeyLayoutArrays();
	void UpdateRenderer();

	void RenderBackground();
	void RenderLines();
	void RenderKeyboard();

	void PushQuad(float x, float y, float width, float height, uint32_t color)
	{
		if (rectDrawCount >= RECT_BUFFER_SIZE)
		{
			RenderQuads(RECT_BUFFER_SIZE);
			rectDrawCount = 0;
		}

		renderRects[rectDrawCount++] = { glm::vec2(x, y), glm::vec2(width, height), glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 0.0f), 0, color & RECT_DATA_MASK };
	}

	// u0,v0,u1,v1 here are fractions (0..1) of the *source image*, not of the
	// padded array canvas - PushQuadLayer below does that remapping for you.
	void PushQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, int layer, uint32_t color = 0xFFFFFFFF)
	{
		if (rectDrawCount >= RECT_BUFFER_SIZE)
		{
			RenderQuads(RECT_BUFFER_SIZE);
			rectDrawCount = 0;
		}

		const glm::vec4& uv = layerUV[layer];
		// Remap [0,1] fractions of the source image into the layer's actual
		// UV rect within the shared canvas.
		glm::vec2 realUV0 = glm::vec2(uv.x + u0 * uv.z, uv.y + v0 * uv.w);
		glm::vec2 realUV1 = glm::vec2(uv.x + u1 * uv.z, uv.y + v1 * uv.w);

		renderRects[rectDrawCount++] = { glm::vec2(x, y), glm::vec2(width, height), realUV0, realUV1, layer, RECT_TEXTURE_FLAG | color};
	}

	void RenderQuads(size_t count);
};