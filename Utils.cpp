#include "Utils.h"
#include <nfd.h>
#include <iostream>
#include <optional>
#include <vector>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cmath>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <cstdlib>
    #include <mach-o/dyld.h>
    #include <limits.h>
#else
    #include <unistd.h>
    #include <cstdlib>
    #include <limits>
#endif

namespace Utils
{
	bool IsMIDIExtension(std::string extension)
	{
		return extension.compare("mid") || extension.compare("midi");
	}

    std::string FormatFilesize(long long bytes, int decimal)
    {
        static const char* units[] =
        {
            "B","KiB","MiB","GiB","TiB","PiB","EiB"
        };

        double d = (double)bytes;
        int i = 0;

        while (i < 6 && d >= 1024.0)
        {
            d /= 1024.0;
            i++;
        }

        std::ostringstream out;

        if (i == 0)
        {
            out << bytes << "B";
        }
        else
        {
            out << std::fixed << std::setprecision(decimal)
                << d << units[i];
        }

        return out.str();
    }

    std::string FormatDuration(double ms)
    {
        long totalSeconds = static_cast<long>(ms / 1000.0);

        long hours = totalSeconds / 3600;
        long minutes = (totalSeconds % 3600) / 60;
        long seconds = totalSeconds % 60;

        std::stringstream stream;

        if (hours > 0)
            stream << std::setfill('0') << std::setw(2) << hours << ":";

        stream << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;

        return stream.str();
    }

    std::string FormatDuration2(double ms)
    {
        if (ms < 0.0)
            ms = 0.0;

        long long totalTenths = static_cast<long long>(std::llround(ms / 100.0));
        long long totalSeconds = totalTenths / 10;
        long long tenths = totalTenths % 10;

        long long hours = totalSeconds / 3600;
        long long minutes = (totalSeconds % 3600) / 60;
        long long secs = totalSeconds % 60;

        std::ostringstream out;

        out << std::setfill('0');

        if (hours >= 10)
        {
            out << std::setw(2) << hours << ":"
                << std::setw(2) << minutes << ":"
                << std::setw(2) << secs << "."
                << tenths;
        }
        else if (hours > 0)
        {
            out << hours << ":"
                << std::setw(2) << minutes << ":"
                << std::setw(2) << secs << "."
                << tenths;
        }
        else if (minutes >= 10)
        {
            out << std::setw(2) << minutes << ":"
                << std::setw(2) << secs << "."
                << tenths;
        }
        else if (minutes > 0)
        {
            out << minutes << ":"
                << std::setw(2) << secs << "."
                << tenths;
        }
        else
        {
            out << secs << "."
                << tenths;
        }

        return out.str();
    }

    bool ChooseFile(std::string& outPath, const char* extension)
    {
        nfdchar_t* outPathRaw = nullptr;
        auto result = NFD_OpenDialog(extension, nullptr, &outPathRaw);
        if (result == NFD_OKAY)
        {
            outPath = outPathRaw;
            free(outPathRaw);
            return true;
        }
        else if (result == NFD_CANCEL)
        {
            return false;
        }

        std::cerr << "Open file dialog failed: " << NFD_GetError() << std::endl;
        return false;
    }

    bool SaveFile(std::string& outPath, const char* extension)
    {
        nfdchar_t* outPathRaw = nullptr;
        auto result = NFD_SaveDialog(extension, nullptr, &outPathRaw);

        if (result == NFD_OKAY)
        {
            outPath = outPathRaw;
            free(outPathRaw);
            return true;
        }
        else if (result == NFD_CANCEL)
        {
            return false;
        }

        std::cerr << "Save file dialog failed: " << NFD_GetError() << std::endl;
        return false;
    }

    bool EqualsIgnoreCase(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++)
        {
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        }
        return true;
    }

    std::optional<ImVec4> ParseColor(std::variant<std::string, uint32_t> strOrInt, std::optional<ImVec4> def)
    {
        const auto hex = [](char c) -> float {
            if (c >= '0' && c <= '9') return (c - '0') / 15.0f;
            if (c >= 'a' && c <= 'f') return (c - 'a' + 10) / 15.0f;
            if (c >= 'A' && c <= 'F') return (c - 'A' + 10) / 15.0f;
            return 0.0f;
        };

        if (std::holds_alternative<std::string>(strOrInt))
        {
            std::string str = std::get<std::string>(strOrInt);
            
            if (!str.empty() && str[0] == '#')
                str.erase(0, 1);
            else if (str.size() >= 2 && (str.starts_with("0x") || str.starts_with("0X")))
                str.erase(0, 2);

            if (str.size() == 3)
            {
                return ImVec4(
                    hex(str[0]),
                    hex(str[1]),
                    hex(str[2]),
                    1.0f
                );
            }

            if (str.size() == 4)
            {
                return ImVec4(
                    hex(str[0]),
                    hex(str[1]),
                    hex(str[2]),
                    hex(str[3])
                );
            }

            const auto parseByte = [&](int i) -> float {
                int hi = (str[i] >= '0' && str[i] <= '9') ? str[i] - '0'
                    : (str[i] >= 'a' && str[i] <= 'f') ? str[i] - 'a' + 10
                    : str[i] - 'A' + 10;

                int lo = (str[i + 1] >= '0' && str[i + 1] <= '9') ? str[i + 1] - '0'
                    : (str[i + 1] >= 'a' && str[i + 1] <= 'f') ? str[i + 1] - 'a' + 10
                    : str[i + 1] - 'A' + 10;

                return ((hi << 4) + lo) / 255.0f;
            };

            if (str.size() == 6)
                return ImVec4(parseByte(0), parseByte(2), parseByte(4), 1.0f);

            if (str.size() == 8)
                return ImVec4(parseByte(0), parseByte(2), parseByte(4), parseByte(6));

            return def;
        }
        else if (std::holds_alternative<uint32_t>(strOrInt))
        {
            uint32_t val = std::get<uint32_t>(strOrInt);
            return ImVec4(
                static_cast<float>((val & 0xFF0000) >> 16) / 255.0f,
                static_cast<float>((val & 0xFF00) >> 8) / 255.0f,
                static_cast<float>(val & 0xFF) / 255.0f,
                1.0
            );
        }
        return def;
    }

    std::string EncodeColor(ImVec4 color)
    {
        std::stringstream enc;
        enc << "0x";
        
        uint32_t red = static_cast<uint32_t>(std::round(color.x * 255));
        uint32_t green = static_cast<uint32_t>(std::round(color.y * 255));
        uint32_t blue = static_cast<uint32_t>(std::round(color.z * 255));
        uint32_t alpha = static_cast<uint32_t>(std::round(color.w * 255));

        red = std::clamp(red, 0u, 255u);
        green = std::clamp(green, 0u, 255u);
        blue = std::clamp(blue, 0u, 255u);
        alpha = std::clamp(alpha, 0u, 255u);

        uint32_t col = (red << 16u) | (green << 8u) | blue;
        if (alpha == 255u)
        {
            enc << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << col;
        }
        else
        {
            col = (col << 8u) | alpha;
            enc << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << col;
        }

        return enc.str();
    }

    std::string DecodeBase64(const std::string& encoded)
    {
        std::vector<int> T(256, 1);
        for (int i = 0; i < 64; i++)
            T[BASE64_CHARS[i]] = i;

        std::string decoded;
        int val = 0, valb = -8;

        for (unsigned char c : encoded) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                decoded.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return decoded;
    }

    void OpenURL(const std::string& url)
    {
#ifdef _WIN32
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
        std::string command = "open " + url;
        system(command.c_str());
#else
        std::string command = "xdg-open " + url;
        system(command.c_str());
#endif
    }

    void OpenFolder(const std::string& path)
    {
        std::error_code ec;
        std::filesystem::path absPath = std::filesystem::absolute(path, ec);
        if (ec)
        {
            std::cerr << "Failed to resolve path: " << ec.message() << std::endl;
            return;
        }

        std::string p = absPath.string();
#if defined(_WIN32)
        std::string command = "explorer \"" + p + "\"";
#elif defined(__APPLE__)
        std::string command = "open \"" + p + "\"";
#elif defined(__linux__)
        std::string command = "xdg-open \"" + p + "\"";
#else
        std::cerr << "Unsupported platform." << std::endl;
        return;
#endif
        std::system(command.c_str());
    }

    bool FolderExists(const std::string& path)
    {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }

    void AddFilePickerField(const char* label, std::filesystem::path& path, const char* extension, bool saving)
    {
        ImGui::Text(label);
        ImGui::SameLine();
        ImGui::PushID(label);
        if (ImGui::Button("Browse..."))
        {
            std::string filePath;
            bool result = saving ? Utils::SaveFile(filePath, extension) : Utils::ChooseFile(filePath, extension);
            if (result)
            {
#if defined(_WIN32) && defined(__cpp_lib_filesystem) && (__cplusplus >= 202002L || _MSVC_LANG >= 202002L)
                path = std::filesystem::path(std::u8string(filePath.begin(), filePath.end()));
#else
                path = std::filesystem::u8path(filePath);
#endif
            }
        }

        ImGui::SameLine();

        auto u8str = path.u8string();
        std::string buf(u8str.begin(), u8str.end());
        ImGui::InputText("##pickedFilePath", buf.data(), buf.size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopID();
    }
}