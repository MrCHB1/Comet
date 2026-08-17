#include "BASSMIDI.h"
#include <algorithm>
#include <vector>
#include <sstream>
#include <iostream>

static WAVEFORMATEX waveFormatStatic = WAVEFORMATEX{};
static int fontCount = 0;
static BASS_MIDI_FONTEX* fontArray = (BASS_MIDI_FONTEX*)malloc(sizeof(BASS_MIDI_FONTEX));
static std::mutex sfLock;
static std::mutex mutex;

void BASSMIDI::InitBASS(WAVEFORMATEX format)
{
	waveFormatStatic = format;
	BASS_Free();

	if (!BASS_Init(-1, waveFormatStatic.nSamplesPerSec, 0, NULL, NULL))
	{
		int err = BASS_ErrorGetCode();
		std::cout << "BASS_Init Failed! Error code: " << err << std::endl;
	}
}

BASSMIDI::BASSMIDI(int voices, bool nofx = true)
{
	streamHandle = BASS_MIDI_StreamCreate(16,
		BASS_SAMPLE_FLOAT |
		BASS_STREAM_DECODE |
		BASS_MIDI_SINCINTER |
		// BASS_MIDI_NOTEOFF1 |
		0x800000,
		waveFormatStatic.nSamplesPerSec);

	if (streamHandle == -1)
	{
		int err = BASS_ErrorGetCode();
		MessageBoxW(NULL, L"BASSMIDI Handle failed to load!\0", L"Error\0", MB_ICONERROR);
	}

	BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_MIDI_VOICES, voices);
	BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_SRC, 3);
	BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_MIDI_CHANS, 16);

	if (nofx) BASS_ChannelFlags(streamHandle, BASS_MIDI_NOFX, BASS_MIDI_NOFX);

	{
		sfLock.lock();
		BASS_MIDI_StreamSetFonts(streamHandle, fontArray, 1);
		sfLock.unlock();
	}
}

void BASSMIDI::FreeSoundfonts()
{
	if (fontArray != NULL)
	{
		for (int i = 0; i < fontCount; i++)
		{
			BASS_MIDI_FontFree((*fontArray).font);
		}
		free(fontArray);
		fontArray = NULL;
		fontCount = 0;
	}
}

void BASSMIDI::LoadSoundfonts(const std::vector<std::wstring>& paths)
{
	sfLock.lock();
	FreeSoundfonts();

	if (paths.empty())
	{
		fontArray = nullptr;
		fontCount = 0;
		sfLock.unlock();
		return;
	}

	fontCount = (int)paths.size();
	BASS_MIDI_FONTEX* fonts = (BASS_MIDI_FONTEX*)malloc(fontCount * sizeof(BASS_MIDI_FONTEX));

	for (int i = 0; i < fontCount; i++)
	{
		HSOUNDFONT font = BASS_MIDI_FontInit((void*)paths[i].c_str(), BASS_UNICODE);
		if (font != 0)
		{
			fonts[0].font = font;
			fonts[0].spreset = -1;
			fonts[0].sbank = -1;
			fonts[0].dpreset = -1;
			fonts[0].dbank = 0;
			fonts[0].dbanklsb = 0;

			BASS_MIDI_FontLoad(font, -1, -1);
		}
		else
		{
			std::stringstream err;
			err << "Soudfont failed to load! Err Code: " << BASS_ErrorGetCode() << "\0";
			std::cout << err.str().c_str() << std::endl;
			fonts[i].font = 0;
		}
	}

	fontArray = fonts;
	sfLock.unlock();
}

bool BASSMIDI::WriteBass(int buflen, unsigned long* progress)
{
	buflen <<= 3;
	unsigned char* buf;

	DWORD ret = BASS_ChannelGetData(streamHandle, &buf, buflen);
	if (ret > 0)
	{
		(*progress) += (unsigned int)ret;
		//
		return true;
	}
	else
	{
		int err = BASS_ErrorGetCode();
		return false;
	}
}

float* BASSMIDI::WriteFloatArray(int buflen, unsigned long* progress)
{
	unsigned char* buf = (unsigned char*)malloc(buflen * 4 * sizeof(unsigned char));
	float* flt = (float*)malloc(buflen * sizeof(float));

	DWORD ret = BASS_ChannelGetData(streamHandle, &buf, buflen * 4);
	if (ret > 0) {
		(*progress) += (unsigned int)ret;
		memcpy(flt, buf, sizeof(buf));
		return flt;
	}
	else
	{
		int err = BASS_ErrorGetCode();
		return nullptr;
	}
}

int BASSMIDI::KShortMessage(int dwParam1, int sampleoffset)
{
	if ((unsigned char)dwParam1 == 0xFF) return 1;

	unsigned char cmd = (unsigned char)dwParam1;

	BASS_MIDI_EVENT ev;

	if (cmd < 0xA0)
	{

		ev.event = MIDI_EVENT_NOTE;
		ev.param = cmd < 0x90 ? (unsigned char)(dwParam1 >> 8) : (unsigned short)(dwParam1 >> 8);
		ev.chan = (int)dwParam1 & 0xF;
		ev.tick = 0;
		ev.pos = sampleoffset << 3;
	}
	else if (cmd < 0xB0)
	{
		ev.event = MIDI_EVENT_KEYPRES;
		ev.param = (unsigned short)dwParam1 >> 8;
		ev.chan = (int)dwParam1 & 0xF;
		ev.tick = 0;
		ev.pos = sampleoffset << 3;
	}
	else if (cmd < 0xC0)
	{
		// TODO
		return 0;
	}
	else if (cmd < 0xD0)
	{
		ev.event = MIDI_EVENT_PROGRAM;
		ev.param = (unsigned char)(dwParam1 >> 8);
		ev.chan = (int)dwParam1 & 0xF;
		ev.tick = 0;
		ev.pos = sampleoffset << 3;
	}
	else if (cmd < 0xE0)
	{
		ev.event = MIDI_EVENT_CHANPRES;
		ev.param = (unsigned char)(dwParam1 >> 8);
		ev.chan = (int)dwParam1 & 0xF;
		ev.tick = 0;
		ev.pos = sampleoffset << 3;
	}
	else if (cmd == 0xF0)
	{
		ev.event = MIDI_EVENT_PITCH;
		ev.param = (int)((unsigned char)(dwParam1 >> 16) | ((dwParam1 & 0x7F00) >> 1));
		ev.chan = (int)dwParam1 & 0xF;
		ev.tick = 0;
		ev.pos = sampleoffset << 3;
	}
	else return 0;

	BASS_MIDI_EVENT evs[1] = { ev };

	BassStreamEvents(evs, 1);

	return 0;
}

DWORD BASSMIDI::Read(float* buffer, int offset, int count) {
	DWORD size = count * sizeof(float);
	DWORD ret = BASS_ChannelGetData(streamHandle, buffer + offset, size | BASS_DATA_FLOAT);

	if (ret == (DWORD)-1)
	{
		int err = BASS_ErrorGetCode();
		MessageBoxW(NULL, L"Error\0", L"Error\0", MB_ICONERROR);
		return 0;
	}
	return ret / 4;
}