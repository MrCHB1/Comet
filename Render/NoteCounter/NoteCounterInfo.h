#pragma once

#include <cstdint>

template <typename T>
struct NoteCounterField
{
	T value = T{};
	bool shown = true;

	NoteCounterField() = default;
	NoteCounterField(T value) : value(value) {}
};

struct NoteCounterInfo
{
	NoteCounterField<uint64_t> notesPassed{ 0 };
	NoteCounterField<uint64_t> polyphony{ 0 };
	NoteCounterField<double> timeSeconds{ 0 };
	NoteCounterField<uint32_t> notesPerSecond{ 0 };
	NoteCounterField<double> bpm{ 120 };
	NoteCounterField<uint64_t> tick{ 0 };
	NoteCounterField<uint16_t> ppq{ 960 };

	void ResetCounter();
};