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
    result.Reserve(totalSize);

    struct TrackHead
    {
        size_t elementIdx;
        uint32_t trackIdx;
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
            heap.push_back({ 0, static_cast<uint32_t>(i), tracks[i].tick[0] });
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
            src.GetTrack(idx),
            src.GetChannel(idx),
            src.tick[idx],
            src.GetKey(idx),
            src.gate[idx],
            src.GetVelocity(idx)
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
    std::vector<NoteSequence> result(MIDI_KEYS);
    const size_t numNotes = notes.Size();

    size_t counts[MIDI_KEYS]{};
    for (size_t n = 0; n < numNotes; n++)
        counts[notes.GetKey(n)]++;

    for (size_t n = 0; n < MIDI_KEYS; n++)
        result[n].Reserve(counts[n]);

    for (size_t i = 0; i < numNotes; ++i)
    {
        uint8_t noteVal = notes.GetKey(i);

        result[noteVal].Emplace(
            notes.GetTrack(i),
            notes.GetChannel(i),
            notes.tick[i],
            noteVal,
            notes.gate[i],
            notes.GetVelocity(i)
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