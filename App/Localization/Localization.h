#pragma once

#include <string>
#include <unordered_map>
#include <map>

#ifndef TRANSLATE
#define TRANSLATE(key) localization->Get(key)
#endif

class Localization
{
public:
	// just loads the list of languages available
	void PreloadLanguages();
	bool LoadLanguage(const std::string& language);
	std::string Get(const std::string& key) const;

	struct LanguageKey
	{
		std::string id;
		std::string label;
	};

	const std::vector<LanguageKey>& GetLanguageList() const { return loadedLanguages; }
	const LanguageKey* GetCurrentLanguage() const { return currLanguage; }
	bool LanguageExists(const std::string& id) const;
private:
	std::unordered_map<std::string, std::string> strings{};
	std::vector<LanguageKey> loadedLanguages{};
	LanguageKey* currLanguage = nullptr;
};