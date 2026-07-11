#include "ResourcePackList.h"
#include <filesystem>

void ResourcePackList::RefreshList()
{
	resourcePacks.clear();
	AddDefaultPack();

	for (const auto& entry : std::filesystem::directory_iterator(RESOURCE_PACK_FOLDER))
	{
		if (!entry.is_directory() && !entry.is_regular_file()) continue;
		std::shared_ptr<ResourcePack> pack = ResourcePack::LoadPack(entry.path().string().c_str());
		if (pack == nullptr) { std::cout << "Couldn't load pack " << entry.path() << std::endl; }
		resourcePacks.push_back(pack);
	}

	if (currPack >= resourcePacks.size()) currPack = resourcePacks.size() - 1;
}

void ResourcePackList::AddDefaultPack()
{
	auto defaultPack = DefaultResourcePack::Instance();
	if (auto pack = std::dynamic_pointer_cast<ResourcePack>(defaultPack))
	{
		resourcePacks.push_back(pack);
	}
}