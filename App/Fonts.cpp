#include "imgui.h"
#include "Fonts.h"
#include "../Inter.hpp"
#include "../FiraCode.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

ImFont* Fonts::DefaultFont = nullptr;
ImFont* Fonts::MonoFont = nullptr;
std::vector<std::vector<char>> Fonts::i18nData;

namespace
{
	namespace fs = std::filesystem;
}

// TODO: monospace i18n fonts too
bool Fonts::LoadI18nData(const std::string& path)
{
	bool success = false;
	
	for (auto& f : fs::directory_iterator(path))
	{
		const fs::path& path = f.path();
		if (!path.stem().string().starts_with("i18n.")) continue;
		success = true;

		std::ifstream f(path, std::ios::binary | std::ios::ate);
		if (!f)
		{
			success = false;
			continue;
		}
		std::streamsize size = f.tellg();
		f.seekg(0);

		std::vector<char> i18n((size_t)size);
		if (!(bool)f.read(i18n.data(), size))
		{
			success = false;
			continue;
		}

		i18nData.push_back(std::move(i18n));
	}

	return success;
}

void Fonts::MergeI18nFonts(float size)
{
	if (i18nData.empty()) return;
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig cfg;
	cfg.MergeMode = true;
	cfg.FontDataOwnedByAtlas = false;  // CJKData outlives the atlas
	cfg.PixelSnapH = true;
	cfg.OversampleH = 1;
	cfg.OversampleV = 1;

	for (std::vector<char>& data : i18nData)
	{
#if IMGUI_VERSION_NUM < 19200
		io.Fonts->AddFontFromMemoryTTF(data.data(), (int)data.size(), size, &cfg,
			io.Fonts->GetGlyphRangesJapanese());
#else
		io.Fonts->AddFontFromMemoryTTF(data.data(), (int)data.size(), size, &cfg);
#endif
	}
}

void Fonts::LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	if (!LoadI18nData("./assets/fonts"))
		std::cout << "One or more localization fonts failed to load. You may see some characters rendered incorrectly.\n";

	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = false;

	DefaultFont = io.Fonts->AddFontFromMemoryTTF((void*)Inter_18pt_Regular_ttf_data, Inter_18pt_Regular_ttf_size, 18.0f, &cfg);
	MergeI18nFonts(18.0f);
	io.FontDefault = DefaultFont;

	MonoFont = io.Fonts->AddFontFromMemoryTTF((void*)FiraCode_Regular_ttf_data, FiraCode_Regular_ttf_size, 18.0f, &cfg);
	MergeI18nFonts(18.0f);
}