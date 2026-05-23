#include "ColorAsset.h"

void ColorAsset::LoadColors()
{
	// for now, just pick random colors
	colors.clear();
	for (int track = 0; track < 16; track++)
	{
		for (int channel = 0; channel < 16; channel++)
		{
			ImVec4 rndColor = CreateRandomColor();
			colors.push_back(rndColor);
		}
	}
}