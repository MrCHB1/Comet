#pragma once

#include "imgui.h"

#define THIN_SLIDER_PERCENT 0.25

class ThinSlider
{
public:
	template <typename T>
	static bool Draw(const char* id, T* v, T min, T max,
		const char* format = "%.3f",
		ImGuiSliderFlags flags = ImGuiSliderFlags_None,
		float heightPercent = THIN_SLIDER_PERCENT,
		bool snapToDefault = false, T defaultValue = T{}, float snapPixels = 6.0f);
};