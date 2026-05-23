#include "Utils.h"
#include <nfd.h>
#include <iostream>
#include <optional>
#include <vector>

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

    void ChooseFile(std::string& outPath)
    {
        nfdchar_t* outPathRaw = nullptr;
        if (NFD_OpenDialog(nullptr, nullptr, &outPathRaw) == NFD_OKAY)
        {
            outPath = outPathRaw;
            free(outPathRaw);
        }
        else
        {
            std::cerr << "Failed to open file dialog: " << NFD_GetError() << std::endl;
        }
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
}