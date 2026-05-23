#pragma once
#include "../ResourcePack/ResourcePack.h"
#include "GPUImage.h"
#include "./RenderEngine/Shaders.h"
#include "./RenderEngine/Buffers.h"
#include <array>
#include "Renderer/QuadDrawer.h"
#include "../MIDI/MIDISequence.h"
#include <mutex>

#pragma region Piano Keyboard Data

#pragma pack(push, 1)
struct RenderKeyboardKey
{
	float left;
	float right;
	uint32_t meta;
};
#pragma pack(pop)

struct KeyboardMeta
{
	static constexpr uint32_t META_PRESSED = 1u << 24;
	static constexpr uint32_t META_BLACK = 1u << 25;

	bool pressed = false;
	bool black = false;
	uint32_t color = 0x000000;

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

#define KEY_IS_BLACK(n) \
( ((n) % 12) == 1 || \
  ((n) % 12) == 3 || \
  ((n) % 12) == 6 || \
  ((n) % 12) == 8 || \
  ((n) % 12) == 10 )

#pragma endregion

const float keyPosDiff[] = {
	0.6F, 0.4F, 0.8F, 0.2F, 1.0F, 0.6F, 0.4F, 0.675F, 0.325F, 0.8F, 0.2F, 1.0F
};

class MIDIRenderer
{
public:
	MIDIRenderer() : keyPos(128), keyWidth(128), keyboardData(128) {}
	void LoadResourcePack(ResourcePack* pack);
	void Initialize();
	void Render();
	void LoadSequence(std::shared_ptr<MIDISequence> seq);
	void UnloadSequence();
	void OnResize(int width, int height);
private:
	std::unique_ptr<GPUImage> textureNote;
	std::unique_ptr<GPUImage> textureNoteEdge;
	std::unique_ptr<GPUImage> textureKeyWhite;
	std::unique_ptr<GPUImage> textureKeyBlack;
	std::unique_ptr<GPUImage> textureKeyWhitePressed;
	std::unique_ptr<GPUImage> textureKeyBlackPressed;

	std::unique_ptr<ShaderProgram> keyboardProgram;
	std::unique_ptr<Buffer> keyboardVBO;
	std::unique_ptr<VertexArray> keyboardVAO;
	std::unique_ptr<Buffer> keyboardIBO;
	std::unique_ptr<Buffer> keyboardEBO;

	std::unique_ptr<Quad> keyboardBackground;

	std::shared_ptr<MIDISequence> seq;

	bool initialized = false;

	std::vector<float> keyPos{};
	std::vector<float> keyWidth{};
	std::vector<RenderKeyboardKey> keyboardData{};
	std::vector<KeyboardMeta> keyMetas{};
	std::vector<uint8_t> kbIDs{};
	float whiteKeyGap = 0.0f;
	float keyboardHeightBlack = 0.0f;
	float keyboardHeightWhite = 0.0f;
	float keyboardHeight = 0.0f;

	int width, height;

	ResourcePack* pack;

	std::mutex renderMutex;

	void CalcKeyPosAndWidth();
	void UpdateKeyboardInstance();
};