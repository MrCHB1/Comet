#include "ResourcePack.h"
#include "FolderResourcePack.h"
#include <filesystem>
#include "../Utils.h"
#include "../Config/ConfigSection.h"
#include <yaml-cpp/yaml.h>

std::shared_ptr<ResourcePack> ResourcePack::LoadPack(const char* file)
{
	std::shared_ptr<ResourcePack> pack = nullptr;
	auto f = std::filesystem::path(file);
	if (std::filesystem::is_directory(f))
	{
		pack = std::make_shared<FolderResourcePack>(file);
	}
	else if (std::filesystem::is_regular_file(f) && Utils::EqualsIgnoreCase(f.extension().string(), "zip"))
	{
		std::cout << ".zip files are currently unsupported." << std::endl;
		return nullptr;
	}

	if (pack == nullptr) return nullptr;

	try
	{
		pack->Init();
	}
	catch (const std::exception& e)
	{
		std::cout << "Failed to initialize pack - " << e.what() << std::endl;
		return nullptr;
	}

	return pack;
}

void ResourcePack::Init()
{
	std::optional<ConfigSection> packMap;
	std::cout << "Initializing pack..." << std::endl;

	try
	{
		auto is = GetStream("pack.yml");
		try
		{
			packMap = ConfigSection(YAML::Load(*(is.get())));
			auto info = std::make_unique<ResourcePackInfo>();
			
			info->format = packMap->GetInt("format", GetFormat());
			info->name = packMap->GetString("name", GetName());
			info->version = packMap->GetString("version", GetVersion());
			info->author = packMap->GetString("author", GetAuthor());
			info->description = packMap->GetString("description", GetDescription());
			// TODO: Signature
			info->signature = packMap->GetString("signature", "");

			// note info section
			ResourcePackInfo::NoteInfo note{};
			std::optional<ConfigSection> noteSec = packMap->GetSection("note");
			if (noteSec.has_value())
			{
				note.borderWidth = noteSec->GetFloat("borderWidth", 0.0);
				note.loopColors = noteSec->GetBoolean("loopColors", false);
			}
			info->note = std::move(note);

			// keyboard section
			ResourcePackInfo::KeyboardInfo keyboard{};
			std::optional<ConfigSection> keyboardSec = packMap->GetSection("keyboard");
			if (keyboardSec.has_value())
			{
				keyboard.whiteKeyGap = keyboardSec->GetFloat("whiteKeyGap", 0.0f);
				keyboard.background = ImVec4(0, 0, 0, 1);
				auto bg = (keyboardSec->GetColor("background"));
				if (bg.has_value())
				{
					keyboard.background = *bg;
				}
			}

			this->info = std::move(info);
		}
		catch (...)
		{

		}
		if (is) is->close();
	}
	catch (...)
	{

	}

	std::optional<ConfigSection> note = packMap->GetSection("note");
	if (note.has_value())
	{
		const std::any* colorsObj = (*note)["colors"];
		if (const auto* col = std::any_cast<ConfigSection::Map>(colorsObj))
		{
			std::cout << "  note color map found" << std::endl;
			info->note.colorMap = {};
			for (auto& [key, value] : *col)
			{
				// unsafe but ok
				int track;
				try
				{
					track = std::atoi(key.c_str());
				}
				catch (...)
				{
					continue;
				}

				if (track < 0) continue;

				std::optional<ImVec4> c = std::nullopt;
				if (const int* v = std::any_cast<int>(&value))
				{
					c = Utils::ParseColor((uint32_t)*v, std::nullopt);
				}
				else if (const std::string* v = std::any_cast<std::string>(&value))
				{
					c = Utils::ParseColor((std::string)*v, std::nullopt);
				}

				if (c != std::nullopt)
				{
					(*(info->note.colorMap))[track] = *c;
				}
			}
			if (info->note.colorMap->empty())
				info->note.colorMap = std::nullopt;
			else
				std::cout << "  loaded " << info->note.colorMap->size() << " color(s)";
		}
		else if (const auto* col = std::any_cast<std::vector<std::any>>(colorsObj))
		{
			std::cout << "  note color list found" << std::endl;
			info->note.colorList = {};
			int i = 0;
			for (auto& obj : *col)
			{
				std::optional<ImVec4> c = std::nullopt;
				if (const int* v = std::any_cast<int>(&obj))
				{
					c = Utils::ParseColor((uint32_t)*v, std::nullopt);
				}
				else if (const std::string* v = std::any_cast<std::string>(&obj))
				{
					c = Utils::ParseColor((std::string)*v, std::nullopt);
				}

				if (c != std::nullopt)
				{
					info->note.colorList->insert(info->note.colorList->begin() + i, *c);
				}
				i++;
			}

			if (info->note.colorList->empty())
				info->note.colorList = std::nullopt;
			else
				std::cout << "  loaded " << info->note.colorList->size() << " color(s)";
		}
	}

	if (!info->signature.empty())
	{
		const char* signBytes;
		try
		{
			signBytes = Utils::DecodeBase64(info->signature).c_str();
		}
		catch (...)
		{
			this->sign = std::make_unique<SignatureState>(::FORGED);
			std::cout << "Failed to parse signature of pack " << GetName() << ", assuming it's forged or corrupted." << std::endl;
			return;
		}

		// TODO: verify signature of pack
	}
}

bool ResourcePack::operator==(const ResourcePack& other)
{
	bool thisNull = file.empty();
	bool otherNull = other.file.empty();
	if (thisNull != otherNull) return false;
	if (thisNull) return true;

	return file == other.file;
}