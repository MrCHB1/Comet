#include "FolderResourcePack.h"
#include "../Utils.h"

std::shared_ptr<std::ifstream> FolderResourcePack::GetStream(const std::string& name)
{
	fs::path fullPath = root / name;
	return std::make_shared<std::ifstream>(fullPath, std::ios::binary);
}

std::vector<std::string> FolderResourcePack::GetEntries()
{
	std::vector<std::string> entries;

	for (const auto& entry : fs::directory_iterator(root))
	{
		if (!entry.is_regular_file()) continue;
		std::string fileName = entry.path().filename().string();
		if (Utils::EqualsIgnoreCase(fileName, "pack.yml")) continue;
		entries.push_back(fileName);
	}

	return entries;
}