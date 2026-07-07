#include "ProgressInputStream.h"
#include "../IO/InputStream.h"
#include "Comet.h"

ProgressInputStream::ProgressInputStream(const char* filePath)
	: InputStream(std::make_shared<std::ifstream>(filePath, std::ios::in | std::ios::binary))
{
	// stream->open(filePath, std::ios::in | std::ios::binary);
	if (!stream->is_open())
	{
		throw std::runtime_error(("Failed to open stream for " + std::string(filePath)).c_str());
	}
	stream->seekg(0, std::ios::end);
	size = stream->tellg();
	stream->seekg(0, std::ios::beg);
	opened = true;
}

ProgressInputStream::ProgressInputStream(InputStream in)
	: InputStream(in.GetStream())
{
	try
	{
		size = in.GetSize();
	}
	catch (std::runtime_error e)
	{
		size = 1;
	}

	opened = true;
}

ProgressInputStream::ProgressInputStream(InputStream in, size_t size)
	: InputStream(in.GetStream()), size(size), opened(true)
{

}

void ProgressInputStream::Read(uint8_t* dst, size_t size)
{
	stream->read((char*)dst, size);
	if (stream->gcount() < size)
	{
		ThrowEos();
	}
	read += stream->gcount();
}

void ProgressInputStream::Seek(int offset, int whence)
{
	stream->clear();
	if (whence == SEEK_CUR)
	{
		stream->seekg(offset, std::ios::cur);
	}
	else if (whence == SEEK_SET)
	{
		stream->seekg(offset, std::ios::beg);
	}
	else if (whence == SEEK_END)
	{
		stream->seekg(offset, std::ios::end);
	}
	else
	{
		throw std::runtime_error("Invalid whence");
	}
	read = stream->tellg();
}