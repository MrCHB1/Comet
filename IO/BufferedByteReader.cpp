#include "BufferedByteReader.h"
#include <cstring>

BufferedByteReader::BufferedByteReader(std::shared_ptr<std::ifstream> stream, size_t start, size_t length, size_t bufferLength, std::mutex* mtx)
{
	this->stream = stream;
	this->mtx = mtx;
	this->bufferLength = bufferLength;
	if (this->bufferLength > length)
	{
		this->bufferLength = length;
	}
	this->bufferStart = start;
	this->bufferPos = 0;
	this->start = start;
	this->length = length;
	this->size = length;
	this->bytes = new uint8_t[this->bufferLength];

	UpdateBuffer();
}

void BufferedByteReader::UpdateBuffer()
{
	const size_t abs = bufferStart + bufferPos;
	const size_t end = start + length;
	const size_t want = abs < end ? std::min(bufferLength, end - abs) : 0;
	size_t got = 0;
	if (want)
	{
		if (mtx) mtx->lock();
		stream->clear();
		stream->seekg((std::streamoff)abs, std::ios::beg);
		stream->read((char*)bytes, (std::streamsize)want);
		got = (size_t)stream->gcount();
		if (mtx) mtx->unlock();
	}
	bufferStart = abs;
	bufferPos = 0;
	bufferValid = got;
	eof = (got == 0);
}

void BufferedByteReader::Seek(size_t offset, int whence)
{
	if (whence != SEEK_SET && whence != SEEK_CUR)
		throw std::runtime_error("Invalid seek whence");

	long long realOffset = offset;
	realOffset += (whence == SEEK_SET) ? start : Pos();

	if (realOffset < (long long)start)
		throw std::runtime_error("Attempted to seek before start");
	if (realOffset > (long long)(start + length))
		throw std::runtime_error("Attempted to seek past end");

	if (bufferStart <= (size_t)realOffset && (size_t)realOffset < bufferStart + bufferValid)
	{
		bufferPos = (size_t)realOffset - bufferStart;
		return;
	}
	bufferStart = realOffset;
	bufferPos = 0;

	UpdateBuffer();
}

void BufferedByteReader::Read(uint8_t* dst, size_t size)
{
	if (Pos() + size > start + length) throw std::runtime_error("Attempted to read past end");
	if (size > bufferLength) throw std::runtime_error("(UNIMPLEMENTED) read size larger than buffer size");

	if (bufferPos + size > bufferValid)
	{
		UpdateBuffer();
		if (bufferValid < size) throw std::runtime_error("Unexpected end of data");
	}
	std::memcpy(dst, bytes + bufferPos, size);
	bufferPos += size;
}

void BufferedByteReader::Skip(size_t size)
{
	const size_t remaining = start + length - Pos();
	Seek(std::min(size, remaining), SEEK_CUR);
}