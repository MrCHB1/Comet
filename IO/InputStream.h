#pragma once
#include <fstream>
#include <cstdint>
#include <memory>
#include <iostream>

class InputStream
{
public:
	InputStream(std::shared_ptr<std::ifstream> stream)
		: stream(stream)
	{
		if (!stream || !stream->is_open())
		{
			std::cerr << "Warning: Stream is null or not opened" << std::endl;
			return;
		}
		size_t currPos = stream->tellg();
		stream->seekg(0, std::ios::end);
		size = stream->tellg();
		stream->seekg(currPos, std::ios::beg);
	}

	size_t GetSize() const
	{
		return size;
	}

	virtual uint8_t ReadByte()
	{
		uint8_t byte;
		Read(&byte, 1);
		return byte;
	}

	virtual void Read(uint8_t* dst, size_t size)
	{
		stream->read((char*)dst, size);
		if (stream->gcount() < size)
		{
			throw std::exception("End of Stream");
		}
	}
	virtual void Seek(int offset, int whence)
	{
		if (whence == std::ios::cur)
		{
			stream->seekg(offset, std::ios::cur);
		}
		else if (whence == std::ios::beg)
		{
			stream->seekg(offset, std::ios::beg);
		}
		else if (whence == std::ios::end)
		{
			stream->seekg(offset, std::ios::end);
		}
		else
		{
			throw std::exception("Invalid whence");
		}
	}
	std::shared_ptr<std::ifstream> GetStream()
	{
		return stream;
	}
	virtual size_t GetPosition()
	{
		return stream->tellg();
	}
	virtual void Close()
	{
		if (stream->is_open()) stream->close();
		else
		{
			std::cout << "Stream is already closed" << std::endl;
		}
	}
	bool IsOpened()
	{
		return stream->is_open();
	}
protected:
	std::shared_ptr<std::ifstream> stream;
	size_t size;
};