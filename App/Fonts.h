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
	static void MergeCJK(float size);
private:
	static std::vector<char> CJKData;
	static bool LoadCJKData(const std::string& path);
};