#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include "RenderEngine/Shaders.h"
#include "RenderEngine/Buffers.h"
#include "MIDI/MIDIDefs.h"
#include <memory>
#include <array>
#include <mutex>

// #define KEY_BUFFER_SIZE MIDI_KEYS * 16
inline constexpr size_t CHANNEL_BUFFER_SIZE = MIDI_KEYS * 16;

#pragma pack(push, 1)
struct RenderChannelKey
{
	int keyChannel;
	uint32_t meta;
	RenderChannelKey() = default;
	RenderChannelKey(int keyChannel, uint32_t meta) : keyChannel(keyChannel), meta(meta) {}
};
#pragma pack(pop)

struct ChannelKeyMeta
{
	static constexpr uint32_t META_ACTIVE = 1u << 24u;

	uint32_t color = 0x000000;
	uint16_t keyChannel;
	bool active = false;

	ChannelKeyMeta() = default;
	ChannelKeyMeta(uint32_t color, bool active)
		: color(color), active(active) {
	}

	constexpr uint32_t GetMeta() const
	{
		return (color & 0x00FFFFFF) |
			(active ? META_ACTIVE : 0);
	}

	void MarkActive(bool active)
	{
		this->active = active;
	}

	void SetKey(uint8_t key)
	{
		keyChannel = (keyChannel & (0xFu << 7)) | static_cast<uint16_t>(key & 0x7Fu);
	}

	void SetChannel(uint8_t channel)
	{
		keyChannel = (keyChannel & 0x7F) | static_cast<uint16_t>(channel & 0xFu) << 7;
	}
};

class MIDIRendererChannels : public AbstractMIDIRenderer
{
public:
	MIDIRendererChannels(MIDIApp* app) : AbstractMIDIRenderer(app)
	{
		renderChannelKeys.fill(RenderChannelKey{});
		channelKeyMetas.fill(ChannelKeyMeta{});
	}

	void Initialize() override;

	void LoadSequence(std::shared_ptr<MIDISequence> seq) override;
	void UnloadSequence() override;
	void Render(double deltaTime) override;
private:
	std::mutex renderMutex;
	double lastTime;

	#pragma region Channel data
	std::unique_ptr<ShaderProgram> channelProgram;
	std::unique_ptr<Buffer> channelVBO;
	std::unique_ptr<VertexArray> channelVAO;
	std::unique_ptr<Buffer> channelIBO;
	std::unique_ptr<Buffer> channelEBO;

	std::array<RenderChannelKey, CHANNEL_BUFFER_SIZE> renderChannelKeys{};
	std::array<ChannelKeyMeta, CHANNEL_BUFFER_SIZE> channelKeyMetas{};
	#pragma endregion

	std::array<size_t, MIDI_KEYS> startRenderIDs{};

	void RenderChannelKeys();
};