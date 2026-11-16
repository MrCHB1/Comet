#pragma once

#include <algorithm>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

#include "../../MIDI/MIDISequence.h"
#include "../RenderEngine/Buffers.h" // needed for getting Framebuffer
#include "../ColorAsset.h"
#include "../NoteCounter/NoteCounterInfo.h"
#include "Config/MIDIPlayerConfig.h"
#include "QuadDrawer.h"
#include "KeyNoteIndex.h"
#include "KeyNoteIndexData.h"

#define LOAD_VAL(n, key, target) \
        if (n && n[key]) { target = n[key].as<std::decay_t<decltype(target)>>(); }

class AbstractMIDIRenderer;
class MIDIApp;

static std::vector<std::shared_ptr<AbstractMIDIRenderer>> RENDERERS = { };

struct InputState;

// Per-frame timing, shared by every falling-note renderer.
struct NoteFrame
{
	double playbackSeconds = 0;
	long   tick = 0;
	double accTime = 0;            // "now" in view units (seconds if timeBased, else ticks)
	double viewRegion = 0;         // window length in view units
	double invViewRegion = 0;
	double invTimeMultiplier = 1;
	double nowTicks = 0;           // "now" in index units (always ticks)
	double viewTicks = 0;
	size_t notesPassed = 0;
	size_t polyphony = 0;
	size_t jobCount = 0;
};

// Geometry of the note area, in pixels. Everything else is derived by the base class.
struct NoteAreaParams
{
	double noteAreaPx;             // height of the falling-note area
	double yBase;                  // pixel row where "now" sits (usually == noteAreaPx)
	double minVisualPx = 0;        // renderer draws notes at least this tall (Synthesia caps)
	bool   pixelSnap = false;      // PFA "rounded note lengths": bottom and height rounded separately
};

// One note that survived culling and de-duplication.
struct NoteRef
{
	uint8_t  id;                   // key
	bool     isBlack;
	uint32_t tick, gate;
	uint16_t track;
	uint8_t  channel;
	double   start, end;           // view units, absolute
	double   yStart, yEnd;         // normalized: (x - accTime) / viewRegion
};

class AbstractMIDIRenderer
{
protected:
	MIDISequence* seq = nullptr;
	std::unique_ptr<Framebuffer> sceneFramebuffer;
	int width = 0;
	int height = 0;
	bool isTimeBased = false;

	std::shared_ptr<NoteCounterInfo> noteCounterInfo;
	std::array<bool, MIDI_KEYS> blackArr{};

	MIDIApp* app = nullptr;
	ColorAsset colors;

	// Fullscreen quad for special effects
	std::unique_ptr<Quad> fullscreenQuad;

	// for 2d falling note renderers only; this helps to optimize the renderers by only giving notes that are actually above others
	std::unique_ptr<KeyNoteIndexData> keyIndexData = nullptr;

	std::array<uint8_t, MIDI_KEYS> kbIDs{};   // white keys first, then black

	static constexpr double kRowMarginPx = 0.02;
	bool BeginNoteFrame(NoteFrame& f);
	void CollectNotes(NoteFrame& f, const NoteAreaParams& area,
		const std::function<void(uint8_t id, uint16_t track, uint8_t channel)>& onActiveKey);
	void EndNoteFrame(const NoteFrame& f);

	template <class Emit>
	void ForEachNote(const NoteFrame& f, Emit&& emit)
	{
		const auto& notes = seq->mergedNotes;
		for (size_t j = 0; j < f.jobCount; ++j)
		{
			const uint8_t id = keyIndexData->jobs[j].id;
			const NoteSequence& ns = notes[id];
			const bool isBlack = blackArr[id];

			bool have = false;
			uint32_t lastTick = 0, lastGate = 0;
			uint16_t lastTrack = 0;
			uint8_t lastChannel = 0;

			for (uint32_t i : keyIndexData->keepPerKey[id])
			{
				const uint32_t tick = ns.tick[i];
				const uint32_t gate = ns.gate[i];

				double start = (double)tick;
				double end = (double)((uint64_t)tick + gate);
				if (isTimeBased) { start *= f.invTimeMultiplier; end *= f.invTimeMultiplier; }

				// safety cull: the index returns a superset
				if (start > f.accTime + f.viewRegion || end <= f.accTime) continue;

				const uint16_t track = ns.GetTrack(i);
				const uint8_t channel = ns.GetChannel(i);

				if (have && tick == lastTick && gate == lastGate &&
					track == lastTrack && channel == lastChannel)
					continue;
				have = true;
				lastTick = tick; lastGate = gate; lastTrack = track; lastChannel = channel;

				emit(NoteRef{ id, isBlack, tick, gate, track, channel, start, end,
					(start - f.accTime) * f.invViewRegion,
					(end - f.accTime) * f.invViewRegion });
			}
		}
	}
public:
	AbstractMIDIRenderer(MIDIApp* app) : app(app)
	{
		for (size_t key = 0; key < MIDI_KEYS; key++)
		{
			size_t k = (key % 12);
			blackArr[key] = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
		}

		size_t n = 0;
		for (size_t k = 0; k < MIDI_KEYS; k++) if (!blackArr[k]) kbIDs[n++] = (uint8_t)k;
		for (size_t k = 0; k < MIDI_KEYS; k++) if (blackArr[k])  kbIDs[n++] = (uint8_t)k;
	}
	virtual ~AbstractMIDIRenderer() = default;

	virtual ColorAsset& GetColorAsset() { return colors; }
	virtual void LoadColors(const std::vector<std::array<float, 3>>& colors)
	{
		if (colors.empty())
		{
			this->colors.ResetColors();
			this->colors.LoadColors();
		}
		else
		{
			this->colors.LoadColors(colors, true);
		}
	}

	// Instead of recalculating key index data every time a 2d renderer is initialized (which is expensive on billion note midis), we would transfer ownership.
	[[nodiscard]] std::unique_ptr<KeyNoteIndexData> TakeKeyIndexData()
	{
		return keyIndexData ? std::move(keyIndexData) : nullptr;
	}

	void SetKeyIndexData(std::unique_ptr<KeyNoteIndexData>&& indexData)
	{
		keyIndexData = std::move(indexData);
	}

	virtual void LoadSequence(MIDISequence* sequence)
	{
		if (!sequence)
		{
			std::cout << "Tried to load a null sequence." << std::endl;
			UnloadSequence();
			return;
		}

		if (!keyIndexData || keyIndexData->sourceSequenceID != sequence->GetSequenceID())
		{
			keyIndexData = std::make_unique<KeyNoteIndexData>();
			keyIndexData->sourceSequenceID = sequence->GetSequenceID();

			const auto* sequences = &sequence->mergedNotes;
			for (std::size_t key = 0; key < MIDI_KEYS; key++)
			{
				const auto* notes = &(sequences->at(key));
				keyIndexData->keyNoteIndices[key].Build(notes->tick.data(), notes->gate.data(), notes->Size());
			}
		}

		seq = sequence;
		isTimeBased = seq->timeBased;
		noteCounterInfo->ppq = sequence->resolution;
	}

	virtual MIDISequence* GetSequence()
	{
		return seq;
	}

	virtual void UnloadSequence()
	{
		if (noteCounterInfo)
			noteCounterInfo->ResetCounter();

		seq = nullptr;
	}

	virtual void Initialize();
	virtual void OnInput(InputState& inputState) { }
	virtual void Render(double deltaTime) = 0;
	virtual void RenderSettings();
	virtual void ResetSettings() {}
	virtual void OnResize(int newWidth, int newHeight)
	{
		width = newWidth;
		height = newHeight;
		if (sceneFramebuffer)
		{
			sceneFramebuffer->Resize(width, height);
		}
	}
	// Usually called when switching between realtime playback and rendering.
	virtual void ResetRenderer() {}

	virtual GLuint GetSceneTexture()
	{
		return sceneFramebuffer ? sceneFramebuffer->GetSceneTexture() : 0;
	}

	virtual void SetNoteCounter(std::shared_ptr<NoteCounterInfo> noteCounterInfo)
	{
		this->noteCounterInfo = noteCounterInfo;
	}

	virtual std::string GetSerializationKey() const = 0;
	virtual YAML::Node GetSettings() { return YAML::Node(); }
	virtual void LoadSettings(const YAML::Node& node) {}
};