#include "SequenceFuncs.h"
#include <array>
#include <vector>

NoteSequence SequenceFuncs::FlattenSequence(std::vector<NoteSequence>&& tracks)
{
    if (tracks.empty()) return {};
    if (tracks.size() == 1) return std::move(tracks[0]);

    size_t totalSize = 0;
    for (const auto& track : tracks)
        totalSize += track.Size();

    NoteSequence result;
    if (totalSize == 0) return result;

    struct TrackHead
    {
        size_t trackIdx;
        size_t elementIdx;
        uint32_t tick;

        bool operator<(const TrackHead& other) const
        {
            if (tick != other.tick)
                return tick > other.tick;
            return trackIdx > other.trackIdx;
        }
    };

    std::vector<TrackHead> heap;
    heap.reserve(tracks.size());

    for (size_t i = 0; i < tracks.size(); i++)
    {
        if (!tracks[i].Empty())
        {
            heap.push_back({ i, 0, tracks[i].tick[0] });
        }
    }

    std::make_heap(heap.begin(), heap.end());

    while (!heap.empty())
    {
        std::pop_heap(heap.begin(), heap.end());
        TrackHead& head = heap.back();

        auto& src = tracks[head.trackIdx];
        size_t idx = head.elementIdx;

        result.Emplace(
            src.track[idx],
            src.channel[idx],
            src.tick[idx],
            src.note[idx],
            src.gate[idx],
            src.vel[idx]
        );

        head.elementIdx++;
        if (head.elementIdx < src.Size())
        {
            head.tick = src.tick[head.elementIdx];
            std::push_heap(heap.begin(), heap.end());
        }
        else
        {
            heap.pop_back();
        }
    }

    return result;
}

std::vector<NoteSequence> SequenceFuncs::DistributeNotes(NoteSequence&& notes)
{
    std::vector<NoteSequence> result(128);
    const size_t numNotes = notes.Size();

    for (size_t i = 0; i < numNotes; ++i)
    {
        uint8_t noteVal = notes.note[i];

        result[noteVal].Emplace(
            notes.track[i],
            notes.channel[i],
            notes.tick[i],
            notes.note[i],
            notes.gate[i],
            notes.vel[i]
        );
    }

    return result;
}

void SequenceFuncs::ApplyTempoEvents(uint16_t ppq, TempoMap* tempoMap, NoteSequence& notes)
{
    const double MAX_SAFE_SECS = 42949.0;
    const size_t size = notes.Size();

    for (size_t i = 0; i < size; ++i)
    {
        long tick = notes.tick[i];

        double secs = tempoMap->TicksToSecsFromMap(ppq, tick);
        notes.tick[i] = std::lround(std::min(secs, MAX_SAFE_SECS) * TIME_BASED_MULTIPLIER);

        double endSecs = tempoMap->TicksToSecsFromMap(ppq, tick + notes.gate[i]);
        double gateSecs = endSecs - secs;
        notes.gate[i] = std::lround(gateSecs * TIME_BASED_MULTIPLIER);
    }
}