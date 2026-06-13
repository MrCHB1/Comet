#include "SequenceFuncs.h"
#include <array>

std::vector<std::vector<NoteEvent>> SequenceFuncs::DistributeNotes(std::vector<NoteEvent>&& notes)
{
    std::vector<std::vector<NoteEvent>> result(128);
    size_t counts[128]{0};
    const size_t numNotes = notes.size();
    NoteEvent* const rawNotes = notes.data();

    for (size_t i = 0; i < numNotes; i++)
        counts[rawNotes[i].note]++;

    for (int i = 0; i < 128; i++)
        if (counts[i] > 0) result[i].reserve(counts[i]);

    for (size_t i = 0; i < numNotes; ++i) {
        result[rawNotes[i].note].emplace_back(std::move(rawNotes[i]));
    }

    return result;
}