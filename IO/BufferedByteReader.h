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

	// BufferedByteReader(const BufferedByteReader&) = delete;
	// BufferedByteReader& operator=(const BufferedByteReader&) = delete;

	void Read(uint8_t* dst, size_t size) override;
	void Skip(size_t nbytes);
	void Seek(size_t offset, int whence) override;
	void Close() override
	{
		InputStream::Close();
		delete[] bytes;
		bytes = nullptr;
	}

	inline uint8_t ReadFast()
	{
		if (bufferPos >= bufferValid) [[unlikely]]
		{
			UpdateBuffer();
			if (bufferValid == 0) return 0;   // end of data: don't touch stale bytes
		}
		return bytes[bufferPos++];
	}
	bool Eof() const { return eof; }
private:
	void UpdateBuffer();
	size_t Pos() const { return bufferStart + bufferPos; }

	uint8_t* bytes = nullptr;
	size_t start = 0, length = 0;
	size_t bufferStart = 0, bufferPos = 0;
	size_t bufferLength = 0;   // capacity
	size_t bufferValid = 0;    // bytes actually filled
	bool eof = false;
	std::mutex* mtx = nullptr;
};