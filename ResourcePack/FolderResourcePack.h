#pragma once
#include "ResourcePack.h"
#include <filesystem>

namespace fs = std::filesystem;

class FolderResourcePack : public ResourcePack
{
public:
	explicit FolderResourcePack(const fs::path& folder) : root(folder) {}

	std::shared_ptr<std::ifstream> GetStream(const std::string& name) override;
	std::vector<std::string> GetEntries() override;
private:
	fs::path root;
};