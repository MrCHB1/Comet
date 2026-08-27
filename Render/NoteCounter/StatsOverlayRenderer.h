#pragma once

#include "AbstractOverlayRenderer.h"
#include "Config/MIDIPlayerConfig.h"

#include <glm/vec2.hpp>

class StatsOverlayRenderer : public AbstractOverlayRenderer
{
public:
	StatsOverlayRenderer(MIDIApp* app) : AbstractOverlayRenderer(app) {}

	bool IsShown() override;

	void Render(float heightOffset) override;
	void OnResize(int width, int height) override;

	glm::vec2 GetOverlayPosition() const override;
	glm::vec2 GetOverlaySize() const override;
private:
	int width = 0, height = 0;
	int statsWidth = 150;
	float lastStatsWidth = 0.0f;
	float lastStatsHeight = 0.0f;
	float lastStatsYOffset = 0.0f;
};