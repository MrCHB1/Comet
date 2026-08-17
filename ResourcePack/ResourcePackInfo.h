#pragma once

#include <string>
#include "imgui.h"
#include <vector>
#include <unordered_map>
#include <optional>

struct ResourcePackInfo
{
	struct NoteInfo
	{
		float borderWidth = 0.0f;
		bool loopColors = false;
		std::optional<std::vector<ImVec4>> colorList = std::nullopt;
		std::optional<std::unordered_map<int, ImVec4>> colorMap = std::nullopt;
	};

	struct KeyboardInfo
	{
		float whiteKeyGap = 0.0f;
		int whiteKeyBorderPixels = 0;
		ImVec4 background = ImVec4(0.0, 0.0, 0.0, 1.0);
	};

	int format = -1;
	std::string name = "No Name!";
	std::string author = "Undefined author";
	std::string version = "No version set";
	std::string description = "No description";
	std::string signature;
	NoteInfo note{};
	KeyboardInfo keyboard{};
};