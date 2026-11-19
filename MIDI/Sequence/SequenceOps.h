#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// REFACTOR (compile-memory): <execution> and the parallel-algorithm backend
// (MSVC pulls in concrt/parallel STL) used to be included by
// MIDI/MultithreadedMIDILoader.cpp, which made the whole MIDI loader translation
// unit pay for the parallel STL front-end. The parallel loops now live behind
// these plain functions and only SequenceOps.cpp includes <execution>.
//
// Behaviour is unchanged: every helper keeps the par_unseq execution policy it
// replaced.

// NOTE: these must be forward declared with the same tag (struct) as their
// definitions, otherwise MSVC mangles the overload names differently and the
// link fails with LNK2019.
struct TempoEvent;
struct TimeSignatureEvent;

namespace SequenceOps
{
	/// Rewrites the packed track-id bits of every element in parallel.
	/// @param packed Packed note values to rewrite in place.
	/// @param mask Mask selecting the track-id bits.
	/// @param value New track-id bits, already shifted into position.
	void RetagPackedTrackIds(std::vector<uint32_t>& packed, uint32_t mask, uint32_t value);

	/// Sorts tempo events ascending by tick using the parallel backend.
	/// @param events Tempo events to sort in place.
	void SortByTick(std::vector<TempoEvent>& events);

	/// Sorts time-signature events ascending by tick using the parallel backend.
	/// @param events Time-signature events to sort in place.
	void SortByTick(std::vector<TimeSignatureEvent>& events);

	/// Invokes `body` once per MIDI key (0..127) using the parallel backend.
	/// @param body Work for a single key; must be safe to run concurrently.
	void ForEachKey(const std::function<void(std::size_t)>& body);

	void ParallelFor(std::size_t count, const std::function<void(std::size_t)>& body);
}
