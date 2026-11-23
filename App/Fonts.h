#pragma once

#include <vector>
#include <string>

struct ImFont;

class Fonts
{
public:
	static ImFont* DefaultFont;
	static ImFont* MonoFont;

	static void LoadFonts();
	static void MergeI18nFonts(float size);
private:
	static std::vector<std::vector<char>> i18nData;
	static bool LoadI18nData(const std::string& path);
};