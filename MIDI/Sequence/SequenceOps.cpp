// REFACTOR (compile-memory): the only translation unit in the project that
// includes <execution>. Everything else calls the thin wrappers in SequenceOps.h.
#include "SequenceOps.h"

#include <algorithm>
#include <array>
#include <execution>
#include <numeric>

#include "MIDI/MIDIDefs.h"
#include "MIDI/Events/TempoEvent.h"
#include "MIDI/Events/TimeSignatureEvent.h"

namespace SequenceOps
{
	void RetagPackedTrackIds(std::vector<uint32_t>& packed, uint32_t mask, uint32_t value)
	{
		std::for_each(std::execution::par_unseq,
			packed.begin(), packed.end(),
			[mask, value](uint32_t& element)
			{
				element = (element & ~mask) | value;
			});
	}

	void SortByTick(std::vector<TempoEvent>& events)
	{
		std::sort(std::execution::par_unseq, events.begin(), events.end(),
			[](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });
	}

	void SortByTick(std::vector<TimeSignatureEvent>& events)
	{
		std::sort(std::execution::par_unseq, events.begin(), events.end(),
			[](const TimeSignatureEvent& a, const TimeSignatureEvent& b) { return a.tick < b.tick; });
	}

	void ForEachKey(const std::function<void(std::size_t)>& body)
	{
		std::array<std::size_t, MIDI_KEYS> indices;
		std::iota(indices.begin(), indices.end(), 0);

		std::for_each(std::execution::par, indices.begin(), indices.end(),
			[&body](std::size_t index)
			{
				body(index);
			});
	}

	void ParallelFor(std::size_t count, const std::function<void(std::size_t)>& body)
	{
		std::vector<std::size_t> idx(count);
		std::iota(idx.begin(), idx.end(), 0);
		std::for_each(std::execution::par, idx.begin(), idx.end(), [&body](std::size_t i) { body(i); });
	}
}
