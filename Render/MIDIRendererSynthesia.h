#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include "GPUImage.h"
#include <vector>

#define RECTS_PER_PASS 16384

#pragma pack(push, 1)
struct TexturedRectInstance
{
	float x;
	float y;
	float width;
	float height;

	float u0, v0;
	float u1, v1;

	uint32_t textureIndex;
};
#pragma pack(pop)

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
	std::vector<TexturedRectInstance> rects;

#pragma region Keyboard textures
	std::unique_ptr<GPUImage> s9Bar;
	std::unique_ptr<GPUImage> s9KeyBlack;
	std::unique_ptr<GPUImage> s9KeyBlackPressed;
	std::unique_ptr<GPUImage> s9KeyWhite;
	std::unique_ptr<GPUImage> s9KeyWhitePressed;
	std::unique_ptr<GPUImage> s9KeyWhiteWhole;
	std::unique_ptr<GPUImage> s9KeyWhiteWholePresesd;
#pragma endregion

#pragma region Note textures
	std::unique_ptr<GPUImage> s10NoteCapTop;
	std::unique_ptr<GPUImage> s10NoteCapBottom;
	std::unique_ptr<GPUImage> s10NoteBody;
#pragma endregion

#pragma region Shadow textures
	std::unique_ptr<GPUImage> s9ShadowLarge;
	std::unique_ptr<GPUImage> s9ShadowUnpressed;
	std::unique_ptr<GPUImage> s9ShadowPressed;
#pragma endregion
};