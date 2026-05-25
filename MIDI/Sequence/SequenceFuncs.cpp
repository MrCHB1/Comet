#include "SequenceFuncs.h"
#include <array>

std::vector<std::vector<NoteEvent>> SequenceFuncs::DistributeNotes(std::vector<NoteEvent>&& notes)
{
    std::vector<std::vector<NoteEvent>> result(128);
    std::array<size_t, 128> counts{};

    for (const auto& note : notes)
        counts[note.note]++;

    for (int i = 0; i < 128; i++)
        result[i].reserve(counts[i]);

    for (NoteEvent& note : notes)
    {
        auto idx = note.note;
        result[idx].push_back(std::move(note));
    }

    return result;
}