#pragma once

#include "AbstractOverlayRenderer.h"
#include "NoteCounterInfo.h"
#include "NoteCounterStyles.h"
#include "Config/MIDIPlayerConfig.h"
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include "imgui.h"

class NoteCounterRenderer : public AbstractOverlayRenderer
{
public:
	NoteCounterRenderer(std::shared_ptr<NoteCounterInfo> noteCounterInfo, MIDIApp* app) : AbstractOverlayRenderer(app), noteCounterInfo(noteCounterInfo) {}
	
	bool IsShown() override;

	void Render(float heightOffset) override;
	void OnResize(int width, int height) override;

	void RenderUMP(float scale);
	void RenderMIDITrail(float scale);

	glm::vec2 GetOverlayPosition() const override;
	glm::vec2 GetOverlaySize() const override;
	float GetCounterHeight() const;
private:
	std::shared_ptr<NoteCounterInfo> noteCounterInfo;

	int width = 0, height = 0;
	int counterWidth = DEFAULT_NOTE_COUNTER_WIDTH;
	float lastCounterYOffset = 0.0f;
	float lastCounterWidth = 0.0f;
	float lastCounterHeight = 0.0f;
};