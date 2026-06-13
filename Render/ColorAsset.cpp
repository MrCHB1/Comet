#include "ColorAsset.h"

void ColorAsset::ResetColors()
{
	colors.clear();
	loopColors = false;
	numLoadedColors = 0;
	hasColors = false;

	LoadColors();
}

uint32_t ColorAsset::GetColor(uint16_t track, uint8_t channel)
{
	if (colors.empty())
		return 0x000000;

	size_t index = ((size_t)track << 4) | (channel & 0xF);

	size_t loadedCount = hasColors
		? std::min((size_t)numLoadedColors, colors.size())
		: 0;

	if (loadedCount == 0)
	{
		index %= colors.size();
	}
	else if (loopColors)
	{
		// loop only the predefined colors
		index %= loadedCount;
	}
	else
	{
		// use predefined colors once
		if (index >= loadedCount)
		{
			size_t generatedCount = colors.size() - loadedCount;

			if (generatedCount > 0)
			{
				index = loadedCount +
					((index - loadedCount) % generatedCount);
			}
			else
			{
				// no generated colors exist
				index %= loadedCount;
			}
		}
	}

	const ImVec4& color = colors[index];

	return ((uint32_t)(color.x * 255.0f) << 16) |
		((uint32_t)(color.y * 255.0f) << 8) |
		(uint32_t)(color.z * 255.0f);
}

void ColorAsset::LoadColors(uint16_t trackCount)
{
	if (trackCount == 0) trackCount = 1;

	const size_t requiredColors = (size_t)trackCount * 16;

	if (hasColors)
	{
		if (loopColors)
		{
			colors.resize(std::min((size_t)numLoadedColors, colors.size()));
			return;
		}

		const size_t loadedCount = std::min((size_t)numLoadedColors, colors.size());
		colors.resize(loadedCount);

		while (colors.size() < requiredColors)
		{
			colors.push_back(CreateRandomColor());
		}
		return;
	}

	colors.clear();
	while (colors.size() < requiredColors)
	{
		colors.push_back(CreateRandomColor());
	}
}

void ColorAsset::LoadColors(const std::vector<std::array<float, 3>>& colors, bool loopColors, size_t requiredCount)
{
	this->loopColors = loopColors;
	this->colors.clear();

	for (auto& color : colors)
	{
		this->colors.emplace_back(color[0], color[1], color[2], 1.0f);
	}
	numLoadedColors = colors.size();
	hasColors = !colors.empty();

	if (!loopColors)
	{
		while (this->colors.size() < requiredCount)
			this->colors.push_back(CreateRandomColor());
	}
}

void ColorAsset::LoadColors(std::shared_ptr<std::istream> paletteFile, bool loopColors)
{
	std::vector<std::array<float, 3>> colors{};

	int width, height, channels;

	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(*paletteFile.get())), std::istreambuf_iterator<char>());
	unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &channels, 4);

	if (!data)
	{
		std::cout << "Failed to load colors. Reverting to randomized colors" << std::endl;
		this->loopColors = false;
		hasColors = false;
		numLoadedColors = 0;
		LoadColors();
		return;
	}

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int idx = (y * width + x) * 4;

			float r = data[idx + 0] / 255.0f;
			float g = data[idx + 1] / 255.0f;
			float b = data[idx + 2] / 255.0f;
			
			colors.push_back({ r, g, b });
		}
	}

	stbi_image_free(data);

	if (colors.empty())
	{
		std::cout << "Palette image contained no colors (for some reason), reverting back to randomized colors" << std::endl;
		this->loopColors = false;
		hasColors = false;
		numLoadedColors = 0;
		LoadColors();
		return;
	}

	LoadColors(colors, loopColors);
}