#pragma once
#include <yaml-cpp/yaml.h>
#include <imgui.h>
#include <string>
#include <memory>

class AppTheme;

namespace CometDefaultThemes
{
    extern std::shared_ptr<AppTheme> DefaultLightMode;
    extern std::shared_ptr<AppTheme> DefaultDarkMode;
    extern std::vector<std::shared_ptr<AppTheme>> DefaultThemesList;
    void InitializeDefaultThemes();
}

struct ThemeColors
{
    ImVec4 text =          ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 textDisabled =  ImVec4(0.10f, 0.10f, 0.10f, 0.50f);

    ImVec4 background =    ImVec4(0.94f, 0.94f, 0.94f, 1.00f); // Window background
    ImVec4 backgroundAlt = ImVec4(0.97f, 0.97f, 0.97f, 1.00f); // Child, Popup backgrounds
    ImVec4 headerBg =      ImVec4(0.92f, 0.92f, 0.92f, 1.00f); // Title bars, Menu bars, Scrollbars

    ImVec4 controlBase =   ImVec4(0.90f, 0.90f, 0.90f, 1.00f); // Buttons, Inactive headers
    ImVec4 controlHover =  ImVec4(0.82f, 0.82f, 0.82f, 1.00f); // Hovered buttons/tabs
    ImVec4 controlActive = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); // Pressed buttons/active tabs

    ImVec4 inputBg =       ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Text input/checkbox backgrounds

    ImVec4 accent =        ImVec4(0.20f, 0.45f, 0.90f, 1.00f); // Checkmarks, slider grabs, plot lines
    ImVec4 accentActive =  ImVec4(0.15f, 0.40f, 0.85f, 1.00f); // Active grabbers

    ImVec4 border =        ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    ImVec4 separator =     ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
};

struct ThemeInfo
{
    std::string name = "Unnamed theme";
    std::string author = "No author";
    std::string description = "No description set.";
    int version = 0;
};

class AppTheme
{
public:
    AppTheme() = default;
    ~AppTheme() = default;

    void ApplyTheme();

    ThemeColors colors;
    ThemeInfo info;
};

class ThemesList
{
public:
    ThemesList(const std::string& themesFolder);
    std::vector<std::shared_ptr<AppTheme>>& GetThemesList()
    {
        return availableThemes;
    }
    AppTheme* GetCurrentTheme()
    {
        return availableThemes[currentTheme].get();
    }
    void SetThemeAndApply(size_t themeIdx)
    {
        if (availableThemes.size() == 0) return;

        if (themeIdx > availableThemes.size())
        {
            SetThemeAndApply(availableThemes.size() - 1);
            return;
        }
        currentTheme = themeIdx;
        GetCurrentTheme()->ApplyTheme();
    }
    void ReloadThemesList();
    void OpenThemeListFolder() const;
private:
    std::shared_ptr<AppTheme> ParseTheme(std::string path);
    ImVec4 ParseColor(const YAML::Node& node, const ImVec4& defaultColor);

    std::string themesPath;
    size_t currentTheme = 0;
    std::vector<std::shared_ptr<AppTheme>> availableThemes = CometDefaultThemes::DefaultThemesList;
};