#pragma once

#include <fstream>
#include <memory>
#include <mutex>
#include "InputStream.h"

class BufferedByteReader : public InputStream
{
public:
	BufferedByteReader(std::shared_ptr<std::ifstream> stream, size_t start, size_t length, size_t bufferLength, std::mutex* mtx);
	~BufferedByteReader()
	{
		delete[] bytes;
		bytes = nullptr;
	}

	BufferedByteReader(const BufferedByteReader&) = delete;
	BufferedByteReader& operator=(const BufferedByteReader&) = delete;

	void Read(uint8_t* dst, size_t size) override;
	void Skip(size_t nbytes);
	void Seek(int offset, int whence) override;
	void Close() override
	{
		InputStream::Close();
		delete[] bytes;
		bytes = nullptr;
	}

	inline uint8_t ReadFast()
	{
		if (bufferPos >= bufferLength) UpdateBuffer();
		pos++;
		return bytes[bufferPos++];
	}
private:
	void UpdateBuffer();

	uint8_t* bytes;
	size_t start;
	size_t pos;
	size_t length;
	size_t bufferStart;
	size_t bufferLength;
	size_t bufferPos;
	std::mutex* mtx;
};