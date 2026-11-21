#include "imgui.h"
#include "Fonts.h"
#include "../Inter.hpp"
#include "../FiraCode.hpp"
#include <fstream>
#include <iostream>

ImFont* Fonts::DefaultFont = nullptr;
ImFont* Fonts::MonoFont = nullptr;
std::vector<char> Fonts::CJKData;

bool Fonts::LoadCJKData(const std::string& path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return false;
	std::streamsize size = f.tellg();
	f.seekg(0);
	CJKData.resize((size_t)size);
	return (bool)f.read(CJKData.data(), size);
}

void Fonts::MergeCJK(float size)
{
	if (CJKData.empty()) return;
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig cfg;
	cfg.MergeMode = true;
	cfg.FontDataOwnedByAtlas = false;  // CJKData outlives the atlas
	cfg.PixelSnapH = true;
	cfg.OversampleH = 1;
	cfg.OversampleV = 1;

#if IMGUI_VERSION_NUM < 19200
	io.Fonts->AddFontFromMemoryTTF(CJKData.data(), (int)CJKData.size(), size, &cfg,
		io.Fonts->GetGlyphRangesJapanese());
#else
	io.Fonts->AddFontFromMemoryTTF(CJKData.data(), (int)CJKData.size(), size, &cfg);
#endif
}

void Fonts::LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	if (!LoadCJKData("./assets/fonts/cjk.ttf"))
		std::cout << "No CJK font (cjk.ttf) found. CJK languages will not render correctly." << std::endl;

	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = false;

	DefaultFont = io.Fonts->AddFontFromMemoryTTF((void*)Inter_18pt_Regular_ttf_data, Inter_18pt_Regular_ttf_size, 18.0f, &cfg);
	MergeCJK(18.0f);
	io.FontDefault = DefaultFont;

	MonoFont = io.Fonts->AddFontFromMemoryTTF((void*)FiraCode_Regular_ttf_data, FiraCode_Regular_ttf_size, 18.0f, &cfg);
	MergeCJK(18.0f);   // optional; remove if you don't need CJK in the mono font
}