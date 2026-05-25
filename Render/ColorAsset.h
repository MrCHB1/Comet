#pragma once
#include "GPUImage.h"
#include <random>
#include <vector>
#include "imgui.h"

class ColorAsset
{
public:
	std::vector<ImVec4> colors{};

	ColorAsset() : mt(rd()), random(0.0f, 1.0f) {}
	void LoadColors();
	uint32_t GetColor(uint16_t track, uint8_t channel)
	{
		if (colors.empty()) return 0x000000;
		const ImVec4& color = colors[(((size_t)track << 4) | (size_t)(channel & 0xF)) % colors.size()];
		return ((uint32_t)(color.x * 255.0f) << 16) |
			((uint32_t)(color.y * 255.0f) << 8) |
			(uint32_t)(color.z * 255.0f);
	}
protected:
	ImVec4 CreateRandomColor()
	{
		ImColor col = ImColor::HSV(random(mt), 1.0f, 0.7f + random(mt) * 0.3f);
		return col.Value;
	}
private:
	std::random_device rd;
	std::mt19937 mt;
	std::uniform_real_distribution<float> random;
};