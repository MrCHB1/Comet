#include "BufferedByteReader.h"

BufferedByteReader::BufferedByteReader(std::shared_ptr<std::ifstream> stream, size_t start, size_t length, size_t bufferLength, std::mutex* mtx)
	: InputStream(stream)
{
	this->mtx = mtx;
	this->bufferLength = bufferLength;
	if (this->bufferLength > length)
	{
		this->bufferLength = length;
	}
	this->bufferStart = 0;
	this->bufferPos = 0;
	this->start = start;
	this->pos = start;
	this->length = length;
	this->bytes = new uint8_t[this->bufferLength];

	UpdateBuffer();
}

void BufferedByteReader::UpdateBuffer()
{
	size_t read = bufferLength;
	if (pos + read > start + length)
	{
		read = start + length - pos;
	}

	if (read == 0 && bufferLength != 0)
		throw std::runtime_error("Outside buffer");

	{
		std::lock_guard<std::mutex> lock(*mtx);
		stream->seekg(pos, std::ios::beg);
		stream->read((char*)bytes, read);
	}

	bufferStart = pos;
	bufferPos = 0;
}

void BufferedByteReader::Seek(int offset, std::ios::seekdir whence)
{
	if (whence != std::ios::beg && whence != std::ios::cur)
		throw std::runtime_error("Invalid seek whence");

	long long realOffset = offset;
	if (whence == std::ios::beg)
		realOffset += start;
	else
		realOffset += pos;

	if (realOffset < (long long)start)
		throw std::runtime_error("Attempted to seek before start");
	if (realOffset > (long long)(start + length))
		throw std::runtime_error("Attempted to seek past end");

	pos = (size_t)realOffset;
	if (bufferStart <= realOffset && realOffset < bufferStart + bufferLength)
	{
		bufferPos = pos - bufferStart;
		return;
	}

	UpdateBuffer();
}

void BufferedByteReader::Read(uint8_t* dst, size_t size)
{
	if (pos + size > start + length)
		throw std::runtime_error("Attempted to read past end");
	if (size > bufferLength)
		throw std::runtime_error("(UNIMPLEMENTED) read size larger than buffer size");

	if (bufferStart + bufferPos + size > bufferStart + bufferLength)
		UpdateBuffer();

	std::memcpy(dst, bytes + bufferPos, size);
	pos += size;
	bufferPos += size;
}

void BufferedByteReader::Skip(size_t size)
{
    Seek(size, std::ios::cur);
}
