#include "Fonts.h"
#include "../Inter.hpp"
#include "../FiraCode.hpp"

ImFont* Fonts::DefaultFont = nullptr;
ImFont* Fonts::MonoFont = nullptr;

void Fonts::LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = false;
	DefaultFont = io.Fonts->AddFontFromMemoryTTF((void*)Inter_18pt_Regular_ttf_data, Inter_18pt_Regular_ttf_size, 18.0f, &cfg);
	io.FontDefault = DefaultFont;
	MonoFont = io.Fonts->AddFontFromMemoryTTF((void*)FiraCode_Regular_ttf_data, FiraCode_Regular_ttf_size, 18.0f, &cfg);
}