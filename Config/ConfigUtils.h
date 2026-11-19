#pragma once

#include <yaml-cpp/yaml.h>
#include "Utils.h"

namespace Config
{
	template <typename T>
	void LoadValue(const YAML::Node& node, const char* key, T& target)
	{
		if (node[key]) target = node[key].as<T>();
	}

	template <typename Vec4>
	void LoadColor(const YAML::Node& node, const char* key,
		std::array<float, 4>& target, const Vec4& def = { 0.0f, 0.0f, 0.0f, 0.0f })
	{
		uint32_t col = Utils::PackRGBA(def.x, def.y, def.z, def.w, Utils::ARGB);
		LoadValue(node, key, col);
		target = Utils::UnpackRGBA(col, Utils::ARGB);
	}
}