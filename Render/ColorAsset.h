#pragma once
#include "../App/MIDIApp.h"
#include "GPUImage.h"
#include <random>
#include <vector>

class ColorAsset
{
public:
	std::vector<ImVec4> colors{};

	ColorAsset(MIDIApp* app) : app(app), mt(rd()), random(0.0f, 1.0f) {}
	void LoadColors();
protected:
	MIDIApp* app;
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