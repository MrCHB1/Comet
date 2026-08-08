#pragma once

#include <vector>
#include <string>
#include <filesystem>

struct ImFont;
struct ImFontAtlas;

inline const char* FONT_FOLDER = "assets/fonts";

struct FontEntry
{
	std::string name = "Default Font";
	std::string path = "";
	ImFont* font = nullptr;
};

class FontList
{
public:
	FontList();
	void ReloadList();
	void ValidateFontList();

	const FontEntry& GetCurrentFont() const
	{
		if (fonts.empty() || currentFont >= fonts.size())
		{
			return defaultFontEntry;
		}
		return fonts[currentFont];
	}

	void SetFont(size_t fontIdx)
	{
		currentFont = fontIdx;
		if (currentFont >= fonts.size() && !fonts.empty())
			currentFont = fonts.size() - 1;
	}

	size_t GetCurrentFontID() const { return currentFont; }
	std::vector<FontEntry>& GetFonts() { return fonts; }

	// must be called before ImGui builds its font atlas (typically during early startup)
	void RegisterFontsIntoAtlas(ImFontAtlas* atlas, float defaultFontSize = 18.0f);

private:
	std::vector<FontEntry> fonts{};
	size_t currentFont = 0;
	FontEntry defaultFontEntry{};

	void CreateDefaultFontsFolder(bool makeFolder = true);
};