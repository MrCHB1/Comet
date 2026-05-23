#pragma once
#include <any>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <optional>
#include <yaml-cpp/yaml.h>
#include <functional>
#include "imgui.h"

class ConfigSection
{
public:
	using Map = std::unordered_map<std::string, std::any>;

	explicit ConfigSection(Map map) : map(std::move(map)) {}
	explicit ConfigSection(const YAML::Node& node);
	const std::any* operator[](const std::string& key) const;
	int GetInt(const std::string& key, int defaultValue) const;
	bool GetBoolean(const std::string& key, bool defaultValue) const;
	float GetFloat(const std::string& key, float defaultValue) const;
	std::string GetString(const std::string& key, const std::string& defaultValue) const;
	std::optional<ImVec4> GetColor(const std::string& key) const;
	std::optional<ConfigSection> GetSection(const std::string& key) const;

	template <typename E>
	E GetEnum(const std::string& key, E defaultValue, std::function<std::optional<E>(const std::string&)> parseEnum) const;
private:
	Map map;

	std::any YamlNodeToAny(const YAML::Node& node);
};