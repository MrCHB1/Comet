#pragma once

#include <mutex>
#include <bass.h>
#include <bassmidi.h>
#include <vector>

class BASSMIDI
{
public:
	HSTREAM streamHandle;

	bool canSeek;
	long long position;
	long long length;

	static void InitBASS(WAVEFORMATEX format);
	static void DisposeBASS() { BASS_Free(); }

	BASSMIDI(int voices, bool nofx);
	~BASSMIDI() { BASS_StreamFree(streamHandle); }

	static void FreeSoundfonts();
	static void LoadSoundfonts(const std::vector<std::wstring>& paths);

	bool WriteBass(int buflen, unsigned long* progress);
	float* WriteFloatArray(int buflen, unsigned long* progress);
	int KShortMessage(int dwParm1, int sampleoffset);
	DWORD SendEvent(DWORD type, DWORD param, DWORD chan, DWORD tick, DWORD time)
	{
		BASS_MIDI_EVENT ev = {
			type = type,
			param = param,
			chan = chan,
			tick = tick,
			time = time << 3
		};
		DWORD mode = BASS_MIDI_EVENTS_TIME | BASS_MIDI_EVENTS_STRUCT;
		return BASS_MIDI_StreamEvents(streamHandle, mode, &ev, 1);
	}

	DWORD SendEventRaw(BYTE data[], DWORD count)
	{
		return BASS_MIDI_StreamEvents(streamHandle, BASS_MIDI_EVENTS_RAW | BASS_MIDI_EVENTS_NORSTATUS, data, count);
	}

	DWORD BassStreamEvents(BASS_MIDI_EVENT events[], int count) {
		DWORD mode = BASS_MIDI_EVENTS_TIME | BASS_MIDI_EVENTS_STRUCT;
		return BASS_MIDI_StreamEvents(streamHandle, mode, events, count);
	}

	DWORD Read(float buffer[], int offset, int count);
};