#include "SequenceFuncs.h"
#include <array>
#include <vector>

NoteSequence SequenceFuncs::FlattenSequence(
    std::vector<NoteSequence>&& tracks, size_t* outProcessed)
{
    if (outProcessed) *outProcessed = 0;
    if (tracks.empty()) return {};
    if (tracks.size() == 1) return std::move(tracks[0]);

    size_t totalSize = 0;
    for (const auto& t : tracks) totalSize += t.Size();

    NoteSequence result;
    if (totalSize == 0) return result;
    result.Resize(totalSize);

    struct Head { uint64_t key; size_t idx; }; // key = tick<<32 | trackIdx
    auto makeKey = [](uint32_t tick, uint32_t t) {
        return (uint64_t(tick) << 32) | t;
        };

    std::vector<Head> heap;
    heap.reserve(tracks.size());
    for (uint32_t i = 0; i < tracks.size(); i++)
        if (!tracks[i].Empty())
            heap.push_back({ makeKey(tracks[i].tick[0], i), 0 });

    auto siftDown = [&](size_t i) {
        const size_t n = heap.size();
        Head h = heap[i];
        for (;;) {
            size_t c = 2 * i + 1;
            if (c >= n) break;
            if (c + 1 < n && heap[c + 1].key < heap[c].key) ++c;
            if (heap[c].key >= h.key) break;
            heap[i] = heap[c];
            i = c;
        }
        heap[i] = h;
        };

    for (size_t i = heap.size() / 2; i-- > 0;) siftDown(i);

    size_t dst = 0;
    while (!heap.empty())
    {
        const uint32_t t = uint32_t(heap[0].key & 0xFFFFFFFFu);
        auto& src = tracks[t];
        const size_t n = src.Size();
        size_t i = heap[0].idx;

        // Runner-up key: everything from this track below it can be bulk-copied
        uint64_t limit = UINT64_MAX;
        if (heap.size() > 1) limit = heap[1].key;
        if (heap.size() > 2 && heap[2].key < limit) limit = heap[2].key;

        size_t j = i + 1;
        while (j < n && makeKey(src.tick[j], t) < limit) ++j;

        const size_t len = j - i;
        if (len == 1)
        {
            result.tick[dst] = src.tick[i];
            result.gate[dst] = src.gate[i];
            result.packed[dst] = src.packed[i];
        }
        else
        {
            std::copy_n(src.tick.data() + i, len, result.tick.data() + dst);
            std::copy_n(src.gate.data() + i, len, result.gate.data() + dst);
            std::copy_n(src.packed.data() + i, len, result.packed.data() + dst);
        }
        dst += len;
        if (outProcessed) (*outProcessed) += len;
        i = j;

        if (i < n)
        {
            heap[0].key = makeKey(src.tick[i], t);
            heap[0].idx = i;
        }
        else
        {
            // free this track's memory as soon as it's exhausted
            src = NoteSequence{};
            heap[0] = heap.back();
            heap.pop_back();
        }
        if (!heap.empty()) siftDown(0);
    }

    tracks.clear();
    return result;
}

std::array<NoteSequence, MIDI_KEYS> SequenceFuncs::DistributeNotes(NoteSequence&& notes, size_t* outNotesProcessed)
{
    if (outNotesProcessed) (*outNotesProcessed) = 0;

    std::array<NoteSequence, MIDI_KEYS> result{};
    const size_t numNotes = notes.Size();

    size_t counts[MIDI_KEYS]{};
    size_t offsets[MIDI_KEYS]{};

    for (size_t n = 0; n < numNotes; n++)
        counts[notes.GetKey(n)]++;

    for (size_t n = 0; n < MIDI_KEYS; n++)
        result[n].Resize(counts[n]);

    for (size_t i = 0; i < numNotes; ++i)
    {
        const uint8_t key = notes.GetKey(i);
        const size_t dst = offsets[key]++;

        result[key].tick[dst] = notes.tick[i];
        result[key].gate[dst] = notes.gate[i];
        result[key].packed[dst] = notes.packed[i];

        if (outNotesProcessed) (*outNotesProcessed)++;
    }

    notes.Clear();
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