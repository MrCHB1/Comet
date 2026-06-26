#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include "imgui.h"
#include <variant>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <type_traits>
#include <chrono>

namespace Utils
{
	bool IsMIDIExtension(std::string extension);
	std::string FormatFilesize(long long bytes, int decimal);
	std::string FormatDuration(double ms);
	std::string FormatDuration2(double ms);
	bool ChooseFile(std::string& outPath, const char* extension = nullptr);
	bool SaveFile(std::string& outPath, const char* extension = nullptr);
	bool EqualsIgnoreCase(const std::string& a, const std::string& b);
	std::optional<ImVec4> ParseColor(std::variant<std::string, uint32_t> strOrInt, std::optional<ImVec4> def);
	std::string EncodeColor(ImVec4 color);
	static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	std::string DecodeBase64(const std::string& encoded);
	void OpenURL(const std::string& url);
	void OpenFolder(const std::string& path);
	bool FolderExists(const std::string& path);
	template <typename T>
	double GetCurrTime()
	{
		return (double)std::chrono::duration_cast<T>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
	template <typename T>
	std::string FormatWithCommas(T value)
	{
		static_assert(std::is_integral_v<T>, "T must be an integer");

		std::string digits = std::to_string(value);

		const bool negative = digits[0] == '-';
		const std::size_t digitCount = digits.size() - negative;
		const std::size_t commaCount = (digitCount - 1) / 3;

		std::string result;
		result.resize(digits.size() + commaCount);

		std::size_t src = digits.size();
		std::size_t dst = result.size();
		int group = 0;

		while (src > static_cast<std::size_t>(negative))
		{
			result[--dst] = digits[--src];

			if (++group == 3 && src > static_cast<std::size_t>(negative))
			{
				result[--dst] = ',';
				group = 0;
			}
		}

		if (negative)
			result[0] = '-';

		return result;
	}
}