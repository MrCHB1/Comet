#pragma once

#include "Renderer/AbstractMIDIRenderer.h"
#include <array>

// Misc renderer for only displaying the note counter.
class MIDIRendererCounter : public AbstractMIDIRenderer
{
public:
	MIDIRendererCounter(MIDIApp* app) : AbstractMIDIRenderer(app) {}
	void Initialize() override;
	void LoadSequence(std::shared_ptr<MIDISequence> sequence) override;
	void Render(double deltaTime) override;
	void OnResize(int width, int height) override;
	void RenderSettings() override
	{
		ImGui::Text("Settings are in the dedicated note counter tab. (Visual > Note Counter)");
		AbstractMIDIRenderer::RenderSettings();
	}

	std::string GetSerializationKey() const override
	{
		return "counterOnly";
	}
private:
	double lastTime = 0.0f;
	std::array<size_t, MIDI_KEYS> startBlockIDs{};
};