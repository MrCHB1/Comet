#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include "RenderEngine/Shaders.h"
#include "RenderEngine/Buffers.h"
#include "MIDI/MIDIDefs.h"
#include <memory>
#include <array>
#include <mutex>

// #define KEY_BUFFER_SIZE MIDI_KEYS * 16
inline constexpr size_t VELOCITY_BUFFER_SIZE = MIDI_KEYS * 128;

#pragma pack(push, 1)
struct RenderVelocityKey
{
	int keyVelocity;
	uint32_t meta;
	RenderVelocityKey() = default;
	RenderVelocityKey(int keyVelocity, uint32_t meta) : keyVelocity(keyVelocity), meta(meta) {}
};
#pragma pack(pop)

struct VelocityKeyMeta
{
	static constexpr uint32_t META_ACTIVE = 1u << 24u;

	uint32_t color = 0x000000;
	uint16_t keyVelocity;
	bool active = false;

	VelocityKeyMeta() = default;
	VelocityKeyMeta(uint32_t color, bool active)
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
		keyVelocity = (keyVelocity & (0x7Fu << 7)) | static_cast<uint16_t>(key & 0x7Fu);
	}

	void SetVelocity(uint8_t velocity)
	{
		keyVelocity = (keyVelocity & 0x7Fu) | (static_cast<uint16_t>(velocity & 0x7Fu) << 7);
	}
};

class MIDIRendererVelocities : public AbstractMIDIRenderer
{
public:
	MIDIRendererVelocities(MIDIApp* app) : AbstractMIDIRenderer(app)
	{
		renderVelocityKeys.fill(RenderVelocityKey{});
		velocityKeyMetas.fill(VelocityKeyMeta{});
	}

	void Initialize() override;

	void LoadSequence(std::shared_ptr<MIDISequence> seq) override;
	void UnloadSequence() override;
	void Render(double deltaTime) override;
private:
	std::mutex renderMutex;
	double lastTime;

#pragma region Velocity data
	std::unique_ptr<ShaderProgram> velocityProgram;
	std::unique_ptr<Buffer> velocityVBO;
	std::unique_ptr<VertexArray> velocityVAO;
	std::unique_ptr<Buffer> velocityIBO;
	std::unique_ptr<Buffer> velocityEBO;

	std::array<RenderVelocityKey, VELOCITY_BUFFER_SIZE> renderVelocityKeys{};
	std::array<VelocityKeyMeta, VELOCITY_BUFFER_SIZE> velocityKeyMetas{};
#pragma endregion

	std::array<size_t, MIDI_KEYS> startRenderIDs{};

	void RenderChannelKeys();
};