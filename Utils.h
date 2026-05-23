#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include "imgui.h"
#include <variant>
#include <cstdint>
#include <optional>

namespace Utils
{
	bool IsMIDIExtension(std::string extension);
	std::string FormatFilesize(long long bytes, int decimal);
	std::string FormatDuration(double ms);
	void ChooseFile(std::string& outPath);
	bool EqualsIgnoreCase(const std::string& a, const std::string& b);
	std::optional<ImVec4> ParseColor(std::variant<std::string, uint32_t> strOrInt, std::optional<ImVec4> def);
	static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	std::string DecodeBase64(const std::string& encoded);
}