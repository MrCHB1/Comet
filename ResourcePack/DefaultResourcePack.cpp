#include "DefaultResourcePack.h"
#include "Comet.h"

void DefaultResourcePack::Init()
{
	info = std::make_unique<ResourcePackInfo>();
	info->format = 1;
	info->name = "Comet Default Pack";
	info->author = "ponluxime";
	info->version = "1.0.0";
	info->description = "The default Comet look";
	info->note.borderWidth = 1.5f;
	info->keyboard.whiteKeyGap = 1.0f;
	info->keyboard.background = ImVec4(0.5, 0.5, 0.5, 1);
}

std::shared_ptr<std::ifstream> DefaultResourcePack::GetStream(const std::string& name)
{
	std::string path = "assets/render/" + name;
#ifdef COMET_DEBUG
	std::cout << "Getting stream for " << path << std::endl;
#endif
	auto stream = std::make_shared<std::ifstream>(path, std::ios::binary);
	if (!stream->is_open())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to get stream for " << path << std::endl;
#endif
		return nullptr;
	}
	return stream;
}

std::vector<std::string> DefaultResourcePack::GetEntries()
{
	return {};
}