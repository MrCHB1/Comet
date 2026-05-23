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
		throw std::exception("Outside buffer");

	{
		std::lock_guard<std::mutex> lock(*mtx);
		stream->seekg(pos, SEEK_SET);
		stream->read((char*)bytes, read);
	}

	bufferStart = pos;
	bufferPos = 0;
}

void BufferedByteReader::Seek(int offset, int whence)
{
	if (whence != SEEK_SET && whence != SEEK_CUR)
		throw std::exception("Invalid seek whence");

	long long realOffset = offset;
	if (whence == SEEK_SET)
		realOffset += start;
	else
		realOffset += pos;

	if (realOffset < (long long)start)
		throw std::exception("Attempted to seek before start");
	if (realOffset > (long long)(start + length))
		throw std::exception("Attempted to seek past end");

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
		throw std::exception("Attempted to read past end");
	if (size > bufferLength)
		throw std::exception("(UNIMPLEMENTED) read size larger than buffer size");

	if (bufferStart + bufferPos + size > bufferStart + bufferLength)
		UpdateBuffer();

	std::memcpy(dst, bytes + bufferPos, size);
	pos += size;
	bufferPos += size;
}

void BufferedByteReader::Skip(size_t size)
{
	Seek(size, SEEK_CUR);
}