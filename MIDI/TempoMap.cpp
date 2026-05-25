#include "TempoMap.h"

void TempoMap::RebuildTempoMap(MIDISequence* seq)
{
	const std::vector<TempoEvent>& tempos = seq->tempos;
	tempoMap.clear();
	if (tempos.size() == 0)
	{
		return;
	}

	double secs = 0;
	int i = 0;
	for (const auto& tempo : tempos)
	{
		long tick = tempo.tick;
		double bpm = tempo.bpm;
		if (i > 0)
		{
			auto& prevTempo = tempos[i - 1];
			long prevTick = prevTempo.tick;
			double prevBpm = prevTempo.bpm;
			double deltaTicks = (double)tick - (double)prevTick;
			secs += deltaTicks * 60.0 / (prevBpm * (double)seq->resolution);
		}

		tempoMap.emplace_back(tick, bpm, secs);
		i++;
	}
}

double TempoMap::TicksToSecsFromMap(uint16_t ppq, long tick)
{
	if (tempoMap.empty())
	{
		return (double)(tick) * 60.0 / (120.0 * (double)ppq);
	}

	auto it = std::upper_bound(
		tempoMap.begin(),
		tempoMap.end(),
		tick,
		[](long t, const TempoPoint& p)
		{
			return t < p.tick;
		}
	);

	size_t idx = (it == tempoMap.begin())
		? 0
		: static_cast<size_t>(std::distance(tempoMap.begin(), it) - 1);

	const TempoPoint& p = tempoMap[idx];
	return p.GetSecsAtTick() + (double)(tick - p.tick) * 60.0 / (p.tempo * (double)ppq);
}

long TempoMap::SecsToTicksFromMap(uint16_t ppq, double secs)
{
	if (tempoMap.empty())
	{
		return static_cast<long>(secs * (120.0 * (double)ppq) / 60.0);
	}

	auto it = std::upper_bound(
		tempoMap.begin(),
		tempoMap.end(),
		secs,
		[](double t, const TempoPoint& p)
		{
			return t < p.GetSecsAtTick();
		}
	);

	size_t idx = (it == tempoMap.begin())
		? 0
		: static_cast<size_t>(std::distance(tempoMap.begin(), it) - 1);

	auto& p = tempoMap[idx];
	double secsPerTick = 60.0 / (p.tempo * (double)ppq);
	double deltaSecs = secs - p.GetSecsAtTick();
	double deltaTicks = deltaSecs / secsPerTick;
	return p.tick + (long)deltaTicks;
}