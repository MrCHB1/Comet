#pragma once

#include <algorithm>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

#include "../../MIDI/MIDISequence.h"
#include "../ColorAsset.h"
#include "../NoteCounter/NoteCounterInfo.h"
#include "Config/MIDIPlayerConfig.h"
#include "QuadDrawer.h"
#include "KeyNoteIndex.h"
#include "KeyNoteIndexData.h"

// #define LOAD_VAL(n, key, target) \
//         if (n && n[key]) { target = n[key].as<std::decay_t<decltype(target)>>(); }

class AbstractMIDIRenderer;
class MIDIApp;
class Framebuffer;

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
	double rowScale = 1.0;         // >1 = finer culling rows (use with anti-aliasing)
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
	struct AdvancedSettings
	{
		double rowMarginPx = 0.001; // 0 = at most one note per pixel row but can introduce some empty pixel rows.
		size_t noteBufferSize = 131072; // Allowed values: 65536, 131072, 262144, 524288
		size_t instancedBufferSize = 128; // Allowed values: 128, 256, 512
	};
	AdvancedSettings advancedSettings{};

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
	AbstractMIDIRenderer(MIDIApp* app);
	virtual ~AbstractMIDIRenderer();

	virtual ColorAsset& GetColorAsset() { return colors; }
	virtual void LoadColors(const std::vector<std::array<float, 3>>& colors);

	// Instead of recalculating key index data every time a 2d renderer is initialized (which is expensive on billion note midis), we would transfer ownership.
	[[nodiscard]] std::unique_ptr<KeyNoteIndexData> TakeKeyIndexData()
	{
		return keyIndexData ? std::move(keyIndexData) : nullptr;
	}

	void SetKeyIndexData(std::unique_ptr<KeyNoteIndexData>&& indexData)
	{
		keyIndexData = std::move(indexData);
	}

	virtual void LoadSequence(MIDISequence* sequence);
	[[nodiscard]] virtual MIDISequence* GetSequence()
	{
		return seq;
	}

	virtual void UnloadSequence();

	virtual void Initialize();
	virtual void OnInput(InputState& inputState) { }
	virtual void Render(double deltaTime) = 0;
	virtual void RenderSettings();
	virtual void ResetSettings() {}
	virtual void OnResize(int newWidth, int newHeight);
	// Usually called when switching between realtime playback and rendering.
	virtual void ResetRenderer() {}

	virtual GLuint GetSceneTexture();
	virtual void SetNoteCounter(std::shared_ptr<NoteCounterInfo> noteCounterInfo)
	{
		this->noteCounterInfo = noteCounterInfo;
	}

	virtual std::string GetSerializationKey() const = 0;
	virtual YAML::Node GetSettings() { return YAML::Node(); }
	virtual void LoadSettings(const YAML::Node& node) {}
};