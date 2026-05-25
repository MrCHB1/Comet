#pragma once

#include "../Events/MIDIEvent.h"
#include "../Events/NoteEvent.h"
#include <vector>

class SequenceFuncs
{
public:
    template <typename T>
    static std::vector<T> MergeSequences(
        std::vector<T>&& seq1,
        std::vector<T>&& seq2)
    {
        static_assert(std::is_base_of_v<MIDIEvent, T>, "T must derive from MIDIEvent.");

        if (seq1.empty()) return std::move(seq2);
        if (seq2.empty()) return std::move(seq1);

        std::vector<T> result;
        result.reserve(seq1.size() + seq2.size());

        T* s1ptr = seq1.data();
        T* s1end = s1ptr + seq1.size();
        T* s2ptr = seq2.data();
        T* s2end = s2ptr + seq2.size();

        while (s1ptr != s1end && s2ptr != s2end)
        {
            if (s1ptr->tick <= s2ptr->tick)
                result.emplace_back(std::move(*s1ptr++));
            else
                result.emplace_back(std::move(*s2ptr++));
        }

        while (s1ptr != s1end)
            result.emplace_back(std::move(*s1ptr++));

        while (s2ptr != s2end)
            result.emplace_back(std::move(*s2ptr++));

        return result;
    }

    template <typename T>
    static std::vector<T> FlattenSequence(std::vector<std::vector<T>>&& tracks)
    {
        static_assert(std::is_base_of_v<MIDIEvent, T>, "T must derive from MIDIEvent.");

        if (tracks.empty()) return {};

        while (tracks.size() > 1)
        {
            std::vector<std::vector<T>> next;
            for (size_t i = 0; i < tracks.size(); i += 2)
            {
                if (i + 1 < tracks.size())
                {
                    next.push_back(
                        MergeSequences(
                            std::move(tracks[i]),
                            std::move(tracks[i + 1])
                        )
                    );
                }
                else
                {
                    next.push_back(std::move(tracks[i]));
                }
            }
            tracks = std::move(next);
        }

        return std::move(tracks[0]);
    }

    static std::vector<std::vector<NoteEvent>> DistributeNotes(std::vector<NoteEvent>&& notes);
};