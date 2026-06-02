#include "ColorAsset.h"

void ColorAsset::LoadColors(uint16_t trackCount)
{
	if (trackCount == 0) trackCount = 1;

	// for now, just pick random colors
	colors.clear();
	for (int track = 0; track < trackCount; track++)
	{
		for (int channel = 0; channel < 16; channel++)
		{
			ImVec4 rndColor = CreateRandomColor();
			colors.push_back(rndColor);
		}
	}
}