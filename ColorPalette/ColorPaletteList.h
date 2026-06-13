#pragma once

#include <vector>
#include <array>
#include <string>
#include <filesystem>

struct ColorPaletteEntry
{
	std::string name = "Uninitialized Palette";
	std::vector<std::array<float, 3>> palette{};
};

class ColorPaletteList
{
public:
	ColorPaletteList();
	void ReloadList();
	void ValidateColorPaletteList();
	const ColorPaletteEntry& GetCurrentPalette() const
	{
		return palettes[currentPalette];
	}
	void SetPalette(size_t paletteIdx) { currentPalette = paletteIdx; }
	std::vector<ColorPaletteEntry>& GetPalettes() { return palettes; }
private:
	std::vector<ColorPaletteEntry> palettes{};
	size_t currentPalette = 0;

	void CreateDefaultColorPalettes(bool makeFolder = true);
	ColorPaletteEntry LoadPaletteFromImage(const std::filesystem::path& path);
};