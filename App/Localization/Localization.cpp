#include "Localization.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "Utils.h"

namespace
{
	using json = nlohmann::json;
	namespace fs = std::filesystem;
}

void Localization::PreloadLanguages()
{
	for (const auto& file : fs::directory_iterator("./assets/locales"))
	{
		const auto& path = file.path();
		if (!Utils::FileExists(path.string())) continue;
		if (path.extension() != ".json") continue;

		std::ifstream file(path, std::ios::in);
		if (!file.is_open()) continue;

        try
        {
            json data;
            file >> data;

            for (auto& [key, value] : data.items())
            {
                if (!value.is_string() || key != "ID") continue;

                std::string langName = value.get<std::string>();

                std::cout << "[Localization] Loaded language \""
                    << langName.c_str() << "\"\n";

                loadedLanguages.emplace_back(
                    LanguageKey{
                        .id = path.stem().string(),
                        .label = std::move(langName)
                    });
                break;
            }
        }
		catch (const json::exception& e)
        {
            std::cout << "[Localization] Failed to load language \"" << path.filename() << "\": " << e.what() << std::endl;
        }

		file.close();
	}
}

bool Localization::LoadLanguage(const std::string& language)
{
    if (!LanguageExists(language))
    {
        std::cout << "[Localization] Language " << language << "does not exist" << std::endl;
        return false;
    }

    // get that language's key
    auto it = std::find_if(
        loadedLanguages.begin(), loadedLanguages.end(),
        [&](const auto& lang) { return lang.id == language; });
    if (it == loadedLanguages.end()) return false;

    std::ifstream file(
        std::format("./assets/locales/{}.json", it->id));

    if (!file.is_open())
    {
        std::cout << "[Localization] Failed to open language file" << std::endl;
        return false;
    }

    try
    {
        json data;
        file >> data;

        std::unordered_map<std::string, std::string> loaded;

        for (auto& [key, value] : data.items())
        {
            if (!value.is_string()) continue;

            std::string val = value.get<std::string>();
            if (key == "ID") continue;
            loaded.emplace(key, std::move(val));
        }

        strings = std::move(loaded);
        currLanguage = &*it;

        return true;
    }
    catch (const json::exception& e)
    {
        std::cout << "[Localization] Failed to load language file: " << e.what() << std::endl;
        return false;
    }
}

std::string Localization::Get(const std::string& key) const
{
	auto it = strings.find(key);
	if (it != strings.end()) return it->second;
	return "[" + key + "]";
}

bool Localization::LanguageExists(const std::string& id) const
{
	return std::any_of(loadedLanguages.begin(), loadedLanguages.end(),
		[&id](const auto& l) { return l.id == id; });
}

