#include "ProgressInputStream.h"
#include "../IO/InputStream.h"
#include "Comet.h"

#ifdef _WIN32
#include <Windows.h>

namespace
{
	std::filesystem::path Utf8ToPath(const std::string& utf8)
	{
		if (utf8.empty())
			return std::filesystem::path();

		int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
		if (wlen <= 0)
			throw std::runtime_error("Failed to convert UTF-8 file path to UTF-16");

		std::wstring wide(wlen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), wlen);
		return std::filesystem::path(wide);
	}
}
#endif

ProgressInputStream::ProgressInputStream(const std::string filePath)
#ifdef _WIN32
	: ProgressInputStream(Utf8ToPath(filePath))
#else
	: ProgressInputStream(std::filesystem::path(filePath))
#endif
{

}

ProgressInputStream::ProgressInputStream(const std::filesystem::path& path)
	: InputStream([&path]() {
#ifdef _WIN32
	return std::make_shared<std::ifstream>(path.wstring(), std::ios::in | std::ios::binary);
#else
	return std::make_shared<std::ifstream>(path, std::ios::in | std::ios::binary);
#endif
		}())
{
	if (!stream->is_open())
	{
		auto u8 = path.u8string();
		std::string pathUtf8(u8.begin(), u8.end());
		throw std::runtime_error("Failed to open stream for " + pathUtf8);
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