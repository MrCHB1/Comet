#include "ColorPaletteList.h"
#include "Render/GPUImage.h"
#include <filesystem>
#include "Utils.h"

ColorPaletteList::ColorPaletteList()
{
	ReloadList();
}

void ColorPaletteList::ReloadList()
{
	ValidateColorPaletteList();
	palettes.clear();
	for (const auto& palette : std::filesystem::directory_iterator("./colors"))
	{
		if (!palette.is_regular_file()) continue;
		if (palette.path().extension().string() != ".png") continue;

		auto p = LoadPaletteFromImage(palette.path().string());
		palettes.push_back(p);
	}
	if (currentPalette >= palettes.size()) currentPalette = palettes.size() - 1;
}

void ColorPaletteList::ValidateColorPaletteList()
{
	if (!Utils::FolderExists("./colors"))
	{
		CreateDefaultColorPalettes();
		return;
	}

	size_t numColors = 0;
	for (const auto& palette : std::filesystem::directory_iterator("./colors"))
	{
		if (!palette.is_regular_file()) continue;
		if (palette.path().extension().string() != ".png") continue;
		numColors++;
	}
	if (numColors == 0) CreateDefaultColorPalettes(false);
}

void ColorPaletteList::CreateDefaultColorPalettes(bool makeFolder)
{
	if (makeFolder) std::filesystem::create_directory("./colors");

	// default rainbow (8 channels)
	{
		std::array<unsigned char, 16 * 8 * 3> colorData{};
		for (int track = 0; track < 8; track++)
		{
			for (int channel = 0; channel < 16; channel++)
			{
				float r = 0.f, g = 0.f, b = 0.f;
				int colorIdx = (channel + track) % 8;
				ImGui::ColorConvertHSVtoRGB((float)colorIdx / 8.0f, 1.0f, 1.0f, r, g, b);

				int imageIdx = ((track << 4) + channel) * 3;
				colorData[imageIdx + 0] = (unsigned char)(r * 255.f);
				colorData[imageIdx + 1] = (unsigned char)(g * 255.f);
				colorData[imageIdx + 2] = (unsigned char)(b * 255.f);
			}
		}
		stbi_write_png("./colors/Rainbow8.png", 16, 8, 3, colorData.data(), 16 * 3);
	}

	// 16-color rainbow
	{
		std::array<unsigned char, 16 * 16 * 3> colorData{};
		for (int track = 0; track < 16; track++)
		{
			for (int channel = 0; channel < 16; channel++)
			{
				float r = 0.f, g = 0.f, b = 0.f;
				int colorIdx = (channel + track) % 16;
				ImGui::ColorConvertHSVtoRGB((float)colorIdx / 16.0f, 1.0f, 1.0f, r, g, b);

				int imageIdx = ((track << 4) + channel) * 3;
				colorData[imageIdx + 0] = (unsigned char)(r * 255.f);
				colorData[imageIdx + 1] = (unsigned char)(g * 255.f);
				colorData[imageIdx + 2] = (unsigned char)(b * 255.f);
			}
		}
		stbi_write_png("./colors/Rainbow16.png", 16, 16, 3, colorData.data(), 16 * 3);
	}
}

ColorPaletteEntry ColorPaletteList::LoadPaletteFromImage(const std::filesystem::path& path)
{
	ColorPaletteEntry palette;
	palette.name = path.filename().string();

	int w, h, c;
	unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &c, 3);

	if (!data) return palette;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			int imageIndex = (y * w + x) * 3;
			std::array<float, 3> color{};
			color[0] = (float)(data[imageIndex + 0]) / 255.0f;
			color[1] = (float)(data[imageIndex + 1]) / 255.0f;
			color[2] = (float)(data[imageIndex + 2]) / 255.0f;
			palette.palette.push_back(color);
		}
	}

	stbi_image_free(data);

	return palette;
}