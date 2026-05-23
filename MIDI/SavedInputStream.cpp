#include "SavedInputStream.h"

uint8_t SavedInputStream::ReadByte()
{
	uint8_t byte = ProgressInputStream::ReadByte();
	dataPos = (dataPos + 1) % data.size();
	data[dataPos] = byte;
	return byte;
}

void SavedInputStream::Read(uint8_t* dst, size_t size)
{
	size_t curr = GetPosition();
	ProgressInputStream::Read(dst, size);
	size_t newPos = GetPosition();

	size_t read = newPos - curr;
	if (read < data.size())
	{
		for (int i = 0; i < read; i++)
		{
			dataPos = (dataPos + 1) % data.size();
			data[dataPos] = dst[i];
		}
	}
	else
	{
		for (int i = read - data.size(), o = 0; i < read; i++, o++)
		{
			data[dataPos] = dst[o];
		}
	}
}