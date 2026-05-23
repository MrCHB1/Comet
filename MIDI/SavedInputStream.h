#pragma once
#include "ProgressInputStream.h"
#include <array>
class SavedInputStream : public ProgressInputStream
{
public:
	SavedInputStream(InputStream is, size_t length)
		: ProgressInputStream(is, length), dataPos(-1)
	{
		data.fill(0);
	}
	SavedInputStream(const char* filePath) : ProgressInputStream(filePath), dataPos(-1)
	{
		data.fill(0);
	}
	
	uint8_t ReadByte() override;
	void Read(uint8_t* dst, size_t size) override;
	void Close() override
	{
		ProgressInputStream::Close();
		opened = false;
	}
private:
	std::array<uint8_t, 64> data{};
	int dataPos;
	std::array<uint8_t, 64> lastMarkedData;
	int lastMarkedPos;
};