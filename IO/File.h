#include <fstream>
#include <filesystem>

class File
{
public:
	std::ifstream stream;
	File() = default;
	
	explicit File(const char* path)
	{
		stream.open(path, std::ios::binary);
		if (!stream.is_open())
		{
			throw std::runtime_error("Failed to open file");
		}

		auto filePath = std::filesystem::path(path);
		if (!std::filesystem::exists(filePath))
		{
			throw std::runtime_error("File does not exist");
		}
	}

	std::ifstream& GetStream()
	{
		return stream;
	}
};