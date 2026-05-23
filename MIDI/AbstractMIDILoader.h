#pragma once
#include "../App/Progress.h"
#include "ProgressInputStream.h"
#include <filesystem>
#include <iostream>
#include "MIDISequence.h"

class AbstractMIDILoader : public Progress
{
public:
	virtual ~AbstractMIDILoader() = default;
	static void Read(std::ifstream* is, size_t size, uint8_t* buf)
	{
		is->read(reinterpret_cast<char*>(buf), size);
	}
	static inline uint32_t ReadVariableLengthValue(ProgressInputStream* is)
	{
		return ReadVariableLengthValue(is);
	}
	static inline uint32_t ReadVariableLengthValue(InputStream* is)
	{
		uint32_t value = 0;
		for (int i = 0; i < 4; i++)
		{
			uint8_t b = is->ReadByte();
			value = (value << 7) | (b & 0x7F);
			if ((b & 0x80) == 0) return value;
		}
		return value;
	}
	static inline uint32_t ReadVariableLengthValue(const uint8_t*& p, const uint8_t* end)
	{
		uint32_t value = 0;
		for (int i = 0; i < 4; i++)
		{
			if (p >= end) throw std::runtime_error("Unexpected end of track while reading VLQ");
			uint8_t b = *p++;
			value = (value << 7) | (b & 0x7F);
			if ((b & 0x80) == 0) return value;
		}
		return value;
	}

	static uint16_t ToShort(uint8_t* bytes)
	{
		uint16_t res = 0;
		for (int i = 0; i < 2; i++)
		{
			res = (res << 8) | static_cast<uint32_t>(bytes[i]);
		}
		return res;
	}
	static uint32_t ToInt(uint8_t* bytes)
	{
		uint32_t res = 0;
		for (int i = 0; i < 4; i++)
		{
			res = (res << 8) | static_cast<uint32_t>(bytes[i]);
		}
		return res;
	}
	static uint64_t ToLong(uint8_t* bytes)
	{
		uint64_t res = 0;
		for (int i = 0; i < 8; i++)
		{
			res = (res << 8) | static_cast<uint64_t>(bytes[i]);
		}
		return res;
	}

	AbstractMIDILoader(const char* name) : Progress(name) {}
	void SetLoadOnlyNotes(bool loadOnlyNotes)
	{
		this->loadOnlyNotes = loadOnlyNotes;
	}
	virtual std::shared_ptr<MIDISequence> Load() = 0;
protected:
	bool loadOnlyNotes = false;
	struct MIDIStreamInfo
	{
		const char* name;

		std::shared_ptr<ProgressInputStream> stream;
	};

	MIDIStreamInfo OpenMIDIFileStream(const char* path)
	{
		auto file = std::filesystem::path(path);
		bool exists = std::filesystem::exists(path);
		if (!exists || std::filesystem::is_directory(path))
		{
			throw std::invalid_argument("File doesn't exist or not a file!");
		}
		MIDIStreamInfo info;
		info.name = std::move(file.filename().string().c_str());
		info.stream = std::make_shared<ProgressInputStream>(path);
		std::cout << "MIDI file stream opened: " << info.name << std::endl;
		return info;
	}
};