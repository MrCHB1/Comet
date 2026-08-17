#include "Themes.h"
#include "Config/ConfigSection.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "Utils.h"

std::shared_ptr<AppTheme> CometDefaultThemes::DefaultLightMode = nullptr;
std::shared_ptr<AppTheme> CometDefaultThemes::DefaultDarkMode = nullptr;
std::vector<std::shared_ptr<AppTheme>> CometDefaultThemes::DefaultThemesList;

void CometDefaultThemes::InitializeDefaultThemes()
{
	DefaultLightMode = std::make_shared<AppTheme>();
	auto& lmInfo = DefaultLightMode->info;
	lmInfo.name = "Comet light mode";
	lmInfo.author = "ponluxime";
	lmInfo.description = "Comet's default light mode theme.";
	lmInfo.version = 1;

	// we don't need to do anything with the default light mode

	DefaultDarkMode = std::make_shared<AppTheme>();
	auto& dmInfo = DefaultDarkMode->info;
	dmInfo.name = "Comet dark mode";
	dmInfo.author = "ponluxime";
	dmInfo.description = "Comet's default dark mode theme.";
	dmInfo.version = 1;

	// ... but we need to change everything with dark mode
	auto& dmColors = DefaultDarkMode->colors;
	dmColors.text = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    dmColors.textDisabled = ImVec4(0.85f, 0.85f, 0.85f, 0.50f);

	dmColors.background = ImVec4(0.22f, 0.22f, 0.22f, 1.00f); // Main window background
	dmColors.backgroundAlt = ImVec4(0.16f, 0.16f, 0.16f, 1.00f); // Child, Popup backgrounds
	dmColors.headerBg = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Title bars, Menu bars

	dmColors.controlBase = ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // Buttons, Inactive headers
	dmColors.controlHover = ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Hovered buttons/tabs
	dmColors.controlActive = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Pressed buttons/active tabs

	dmColors.inputBg = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Text input/checkbox backgrounds

	dmColors.accent = ImVec4(0.18f, 0.43f, 0.71f, 1.00f); // Checkmarks, slider grabs (Unity Blue)
	dmColors.accentActive = ImVec4(0.22f, 0.50f, 0.82f, 1.00f); // Active grabbers (Brighter blue)

	dmColors.border = ImVec4(0.10f, 0.10f, 0.10f, 1.00f); // Dark, crisp borders
	dmColors.separator = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    DefaultThemesList.push_back(DefaultLightMode);
    DefaultThemesList.push_back(DefaultDarkMode);
}

void AppTheme::ApplyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // -----------------------
    // Layout / feel
    // -----------------------
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.ItemSpacing = ImVec2(8, 6);
    style.WindowPadding = ImVec2(10, 10);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    // -----------------------
    // Colors
    // -----------------------
    ImVec4* c = style.Colors;

    c[ImGuiCol_Text] = colors.text;
    c[ImGuiCol_TextDisabled] = colors.textDisabled;

    // Backgrounds
    c[ImGuiCol_WindowBg] = colors.background;
    c[ImGuiCol_ChildBg] = colors.backgroundAlt;
    c[ImGuiCol_PopupBg] = colors.backgroundAlt;
    c[ImGuiCol_DockingEmptyBg] = colors.background;

    // Title / Menu / Scrollbar bases
    c[ImGuiCol_TitleBg] = colors.headerBg;
    c[ImGuiCol_TitleBgCollapsed] = colors.headerBg;
    c[ImGuiCol_MenuBarBg] = colors.headerBg;
    c[ImGuiCol_ScrollbarBg] = colors.headerBg;

    // Borders
    c[ImGuiCol_Border] = colors.border;
    c[ImGuiCol_Separator] = colors.separator;

    // Inputs / Frames
    c[ImGuiCol_FrameBg] = colors.inputBg;
    c[ImGuiCol_FrameBgHovered] = colors.controlBase;
    c[ImGuiCol_FrameBgActive] = colors.controlHover;

    // Buttons
    c[ImGuiCol_Button] = colors.controlBase;
    c[ImGuiCol_ButtonHovered] = colors.controlHover;
    c[ImGuiCol_ButtonActive] = colors.controlActive;

    // Headers (Tree nodes, collapsibles)
    c[ImGuiCol_Header] = colors.controlBase;
    c[ImGuiCol_HeaderHovered] = colors.controlHover;
    c[ImGuiCol_HeaderActive] = colors.controlActive;

    // Tabs
    c[ImGuiCol_Tab] = colors.controlBase;
    c[ImGuiCol_TabHovered] = colors.controlHover;
    c[ImGuiCol_TabSelected] = colors.controlActive;
    c[ImGuiCol_TabUnfocused] = colors.controlBase;
    c[ImGuiCol_TabUnfocusedActive] = colors.controlHover;

    // Scrollbar Grabs
    c[ImGuiCol_ScrollbarGrab] = colors.controlActive;
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(std::max(colors.controlActive.x - 0.1f, 0.0f), std::max(colors.controlActive.y - 0.1f, 0.0f), std::max(colors.controlActive.z - 0.1f, 0.0f), 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(std::max(colors.controlActive.x - 0.2f, 0.0f), std::max(colors.controlActive.y - 0.2f, 0.0f), std::max(colors.controlActive.z - 0.2f, 0.0f), 1.0f);

    // Accents
    c[ImGuiCol_CheckMark] = colors.accent;
    c[ImGuiCol_SliderGrab] = colors.accent;
    c[ImGuiCol_SliderGrabActive] = colors.accentActive;
    c[ImGuiCol_TabSelectedOverline] = colors.accent;
    c[ImGuiCol_PlotLines] = colors.accent;
    c[ImGuiCol_PlotHistogram] = colors.accent;

    // Resize Grips
    c[ImGuiCol_ResizeGrip] = ImVec4(colors.border.x, colors.border.y, colors.border.z, 0.30f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(colors.accent.x, colors.accent.y, colors.accent.z, 0.70f);
    c[ImGuiCol_ResizeGripActive] = colors.accent;

    // Other
    c[ImGuiCol_TitleBgActive] = colors.controlBase;
    c[ImGuiCol_DockingPreview] = ImVec4(colors.accent.x, colors.accent.y, colors.accent.z, 0.25f);
}

ThemesList::ThemesList(const std::string& themesFolder)
{
    this->themesPath = themesFolder;
    
    if (!std::filesystem::exists(themesFolder))
    {
        std::filesystem::create_directory(themesFolder);
    }

    ReloadThemesList();
}

#define LOAD_THEME_COLOR(field, yamlLabel) themeColors.field = ParseColor(themeSec[yamlLabel], themeColors.field)

std::shared_ptr<AppTheme> ThemesList::ParseTheme(std::string path)
{
    std::optional<ConfigSection> theme;
    std::shared_ptr<AppTheme> parsedTheme = std::make_shared<AppTheme>();
    try
    {
        std::ifstream stream(path, std::ios::in);
        if (!stream.is_open())
        {
            std::cout << "Failed to open theme at path " << path << ". Returning default light mode theme." << std::endl;
            return CometDefaultThemes::DefaultLightMode;
        }

        YAML::Node themeYml = YAML::Load(stream);
        theme = ConfigSection(themeYml);
        std::optional<ConfigSection> infoSec = theme->GetSection("information");
        if (infoSec)
        {
            ThemeInfo themeInfo;
            themeInfo.author = infoSec->GetString("author", themeInfo.author);
            themeInfo.name = infoSec->GetString("name", themeInfo.name);
            themeInfo.description = infoSec->GetString("description", themeInfo.description);
            themeInfo.version = infoSec->GetInt("version", themeInfo.version);
            
            parsedTheme->info = std::move(themeInfo);
        }

        std::optional<ConfigSection> themeSec = theme->GetSection("theme");
        if (themeSec)
        {
            ThemeColors themeColors;
            YAML::Node themeSec = themeYml["theme"];

            LOAD_THEME_COLOR(text, "text");
            LOAD_THEME_COLOR(textDisabled, "textDisabled");

            LOAD_THEME_COLOR(background, "background");
            LOAD_THEME_COLOR(backgroundAlt, "backgroundAlt");
            LOAD_THEME_COLOR(headerBg, "headerBackground");

            LOAD_THEME_COLOR(controlBase, "controlBase");
            LOAD_THEME_COLOR(controlHover, "controlHover");
            LOAD_THEME_COLOR(controlActive, "controlActive");

            LOAD_THEME_COLOR(inputBg, "inputBackground");
            LOAD_THEME_COLOR(accent, "accent");
            LOAD_THEME_COLOR(accentActive, "accentActive");

            LOAD_THEME_COLOR(border, "borderColor");
            LOAD_THEME_COLOR(separator, "separatorColor");

            parsedTheme->colors = std::move(themeColors);
        }
    }
    catch (...)
    {
        std::cout << "Failed to parse theme at path " << path << "." << std::endl;
    }

    return parsedTheme;
}

void ThemesList::ReloadThemesList()
{
    availableThemes = CometDefaultThemes::DefaultThemesList;
    for (const auto& file : std::filesystem::directory_iterator(this->themesPath))
    {
        if (!file.is_regular_file()) continue;
        if (file.path().filename().extension() != ".yml") continue;
        std::shared_ptr<AppTheme> parsedTheme = ParseTheme(file.path().string());
        availableThemes.push_back(parsedTheme);
    }
}


ImVec4 ThemesList::ParseColor(const YAML::Node& node, const ImVec4& defaultColor)
{
    if (node && node.IsSequence() && node.size() >= 3)
    {
        float r = node[0].as<float>();
        float g = node[1].as<float>();
        float b = node[2].as<float>();
        float a = node.size() == 4 ? node[3].as<float>() : 1.0f;
        return ImVec4(r, g, b, a);
    }
    return defaultColor;
}

void ThemesList::OpenThemeListFolder() const
{
    Utils::OpenFolder(themesPath);
}