#pragma once

#include <cstdint>

// REFACTOR (compile-memory): RenderNote used to live in Render/MIDIRenderer.h,
// which meant every translation unit that only needed this five-float vertex
// struct (the MIDI-trail renderer, for example) pulled the entire renderer hub:
// ResourcePack.h, ColorAsset.h, QuadDrawer.h, NoteCounterInfo.h, ThreadPool.h
// and KeyNoteIndex.h. Extracting the struct keeps that dependency fan-out out of
// those translation units. The struct layout below is unchanged (packed, 20
// bytes) so the GPU vertex format is byte-identical.

#pragma pack(push, 1)
/// Per-note instance data uploaded to the note shader (one note per instance).
struct RenderNote
{
	float left;   ///< Left edge of the note, in key units.
	float right;  ///< Right edge of the note, in key units.
	float start;  ///< Start position along the time axis.
	float end;    ///< End position along the time axis.
	uint32_t color; ///< Packed 0x00RRGGBB note colour.

	RenderNote() = default;
	RenderNote(float left, float right, float start, float end, uint32_t color)
		: left(left), right(right), start(start), end(end), color(color) {
	}
};
#pragma pack(pop)
