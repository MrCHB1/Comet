#pragma once

#include <stdexcept>
#include "../IO/InputStream.h"

class ProgressInputStream : public InputStream
{
public:
	ProgressInputStream(const char* file);
	ProgressInputStream(InputStream in);
	ProgressInputStream(InputStream in, size_t size);
	~ProgressInputStream()
	{
		Close();
	}

	void Read(uint8_t* dst, size_t size) override;
	void Seek(int offset, int whence) override;
	void Close() override
	{
		InputStream::Close();
		opened = false;
	}

	size_t GetPosition() override
	{
		return read;
	}

	size_t GetSize()
	{
		return size;
	}

	double GetProgress()
	{
		return (double)read / (double)size;
	}

	bool DidReadAfterEos()
	{
		return eos;
	}
protected:
	bool opened = false;
	bool eos = false;
	size_t read = 0;
	size_t size = 0;
private:
	void ThrowEos()
	{
		eos = true;
		throw std::runtime_error("End of Stream");
	}
};