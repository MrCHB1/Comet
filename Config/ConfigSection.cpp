#include "ConfigSection.h"
#include "../Utils.h"

ConfigSection::ConfigSection(const YAML::Node& node)
{
	Map map;
	if (node && node.IsMap())
	{
		for (auto it = node.begin(); it != node.end(); ++it)
		{
			std::string key = it->first.as<std::string>();
			map.emplace(std::move(key), YamlNodeToAny(it->second));
		}
	}

	this->map = map;
}


const std::any* ConfigSection::operator[](const std::string& key) const
{
	auto it = map.find(key);
	return (it != map.end()) ? &it->second : nullptr;
}

int ConfigSection::GetInt(const std::string& key, int defaultValue) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return defaultValue;
	if (auto v = std::any_cast<int>(obj)) return *v;
	return defaultValue;
}

bool ConfigSection::GetBoolean(const std::string& key, bool defaultValue) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return defaultValue;
	if (auto v = std::any_cast<bool>(obj)) return *v;
	return defaultValue;
}

float ConfigSection::GetFloat(const std::string& key, float defaultValue) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return defaultValue;
	if (auto v = std::any_cast<float>(obj)) return *v;
	return defaultValue;
}

std::string ConfigSection::GetString(const std::string& key, const std::string& defaultValue) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return defaultValue;
	if (auto v = std::any_cast<std::string>(obj)) return *v;
	if (auto v = std::any_cast<const char*>(obj)) return std::string(*v);
	return defaultValue;
}

std::optional<ImVec4> ConfigSection::GetColor(const std::string& key) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return std::nullopt;

	if (auto p = std::any_cast<uint32_t>(obj))
		return Utils::ParseColor(*p, std::nullopt);
	else if (auto p = std::any_cast<std::string>(obj))
		return Utils::ParseColor(*p, std::nullopt);
	else
		return std::nullopt;
}

std::optional<ConfigSection> ConfigSection::GetSection(const std::string& key) const
{
	const std::any* obj = (*this)[key];
	if (!obj) return std::nullopt;
	if (const auto* m = std::any_cast<Map>(obj)) return ConfigSection(*m);
	return std::nullopt;
}

template <typename E>
E ConfigSection::GetEnum(const std::string& key, E defaultValue, std::function<std::optional<E>(const std::string&)> parseEnum) const
{
	std::string name = GetString(key, "");
	if (name.empty()) return defaultValue;
	if (auto parsed = parseEnum(name)) return *parsed;
	return defaultValue;
}

std::any ConfigSection::YamlNodeToAny(const YAML::Node& node)
{
	if (!node || node.IsNull()) return std::any{};

	if (node.IsMap())
	{
		Map map;

		for (auto it = node.begin(); it != node.end(); ++it)
		{
			std::string key = it->first.as<std::string>();
			map.emplace(std::move(key), YamlNodeToAny(it->second));
		}

		return map;
	}

	if (node.IsSequence())
	{
		std::vector<std::any> arr;
		arr.reserve(node.size());

		for (const auto& item : node)
			arr.push_back(YamlNodeToAny(item));

		return arr;
	}

	if (node.IsScalar())
	{
		try { return node.as<int>(); } catch (...) {}
		try { return node.as<float>(); } catch (...) {}
		try { return node.as<bool>(); } catch (...) {}
		return node.as<std::string>();
	}

	return std::any{};
}