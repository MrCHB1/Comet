#include "AbstractMIDIRenderer.h"
#include "App/MIDIApp.h"
#include "PrimitiveShaders.h"
#include "App/Dialog/DialogMacros.h"
#include "App/MainWindow.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"

#include "Render/RenderEngine/Buffers.h"
#include "App/UI/Layout/ForumLayout.h"

namespace
{
	using namespace Comet::UI::Layout;
}

AbstractMIDIRenderer::AbstractMIDIRenderer(MIDIApp* app) : app(app)
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

AbstractMIDIRenderer::~AbstractMIDIRenderer() = default;

void AbstractMIDIRenderer::LoadColors(const std::vector<std::array<float, 3>>& colors)
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

void AbstractMIDIRenderer::LoadSequence(MIDISequence* sequence)
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

void AbstractMIDIRenderer::UnloadSequence()
{
	if (noteCounterInfo)
		noteCounterInfo->ResetCounter();
	seq = nullptr;
}

void AbstractMIDIRenderer::Initialize()
{
	MIDIPlayerConfig* cfg = app->GetConfig();
	int width = cfg->render.GetWidth();
	int height = cfg->render.GetHeight();

	#pragma region Framebuffer creation

	sceneFramebuffer = std::make_unique<Framebuffer>();
	sceneFramebuffer->Setup(width, height);
	fullscreenQuad = std::make_unique<Quad>();
	fullscreenQuad->SetShader(SCENE_SHADER);
	BLUR_SHADER->SetFloat("width", (float)width);
	BLUR_SHADER->SetFloat("height", (float)height);
	fullscreenQuad->SetTransform({ glm::vec3(0.0f), glm::vec2(1.0f) }, false);

	#pragma endregion
}

bool AbstractMIDIRenderer::BeginNoteFrame(NoteFrame& f)
{
	f = {};
	if (!seq || !keyIndexData || seq->mergedNotes.empty()) return false;
	auto* renderView = app->GetRenderView();
	if (!renderView) return false;

	f.playbackSeconds = app->GetTimer()->Elapsed();

	TempoMap* tempoMap = seq->GetTempoMap();
	f.tick = tempoMap->SecsToTicksFromMap(seq->resolution, f.playbackSeconds);
	noteCounterInfo->tick.value = f.tick >= 0 ? f.tick : 0;
	noteCounterInfo->timeSeconds.value = f.playbackSeconds;
	noteCounterInfo->bpm.value = tempoMap->GetBPMAtTick(f.tick);

	const double tickScale = isTimeBased ? (double)TIME_BASED_MULTIPLIER : 1.0;
	f.accTime = isTimeBased ? f.playbackSeconds : (double)f.tick;
	f.viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000.0 : (double)renderView->viewTicks;
	f.invViewRegion = 1.0 / f.viewRegion;
	f.invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;
	f.nowTicks = f.accTime * tickScale;
	f.viewTicks = f.viewRegion * tickScale;
	return true;
}

void AbstractMIDIRenderer::CollectNotes(NoteFrame& f, const NoteAreaParams& area,
	const std::function<void(uint8_t, uint16_t, uint8_t)>& onActiveKey)
{
	KeyNoteIndexData& kd = *keyIndexData;
	const auto& notes = seq->mergedNotes;

	MIDIPlayerConfig* config = app->GetConfig();
	const int keyFirst = config->render.GetKeyFirst();
	const int keyLast = config->render.GetKeyLast();
	const int startRender = blackArr[keyFirst] ? keyFirst - 1 : keyFirst;
	const int endRender = blackArr[keyLast] ? keyLast + 1 : keyLast;
	const size_t sparseLimit = (size_t)height / 4;

	// pass 1 (serial): stats, pressed keys, job list
	f.jobCount = 0; f.notesPassed = 0; f.polyphony = 0;
	for (uint8_t id : kbIDs)
	{
		const NoteSequence& ns = notes[id];
		if (ns.Size() == 0) continue;
		auto& index = kd.keyNoteIndices[id];

		const size_t started = index.CountStartAtOrBefore(f.nowTicks);
		const size_t ended = index.CountEndAtOrBefore(f.nowTicks);
		f.notesPassed += started;
		f.polyphony += started - ended;

		const ptrdiff_t active = index.LastActiveAt(f.nowTicks);
		if (active >= 0 && onActiveKey)
			onActiveKey(id, ns.GetTrack(active), ns.GetChannel(active));

		if (id < startRender || id > endRender) continue;

		const size_t startCap = index.CountStartAtOrBefore(f.nowTicks + f.viewTicks);
		const size_t visible = index.CountVisible(f.nowTicks, startCap, ended);
		if (visible == 0) continue;

		kd.jobs[f.jobCount++] = { id, startCap, visible <= sparseLimit };
	}

	// pass 2 (parallel): collect per-key indices
	const double ticksPerPixel = f.viewTicks / area.noteAreaPx;
	// 0.499 rather than 0.5: slack for the shader's float math
	const uint32_t minGate = area.pixelSnap ? (uint32_t)std::ceil(0.499 * ticksPerPixel) : 0u;
	const double m = area.minVisualPx * ticksPerPixel;
	const size_t jobCount = f.jobCount;

	kd.nextJob.store(0, std::memory_order_relaxed);
	auto worker = [&]
		{
			for (size_t j; (j = kd.nextJob.fetch_add(1, std::memory_order_relaxed)) < jobCount;)
			{
				const auto& job = kd.jobs[j];
				auto& index = kd.keyNoteIndices[job.id];
				auto& out = kd.keepPerKey[job.id];

				if (job.sparse)
					index.CollectVisible(out, f.nowTicks, job.startCap, minGate, m);
				else
					index.CollectRowOwners(out, RowQuery{
						.tNow = f.nowTicks, .ticksPerPixel = ticksPerPixel,
						.yBase = area.yBase, .rowBegin = 0, .rowEnd = (int)height,
						.marginPx = advancedSettings.rowMarginPx, .startCap = job.startCap,
						.minGate = minGate, .minVisualPx = area.minVisualPx,
						.pixelSnap = area.pixelSnap });
			}
		};

	kd.collectFutures.clear();
	const size_t extra = std::min(kd.threadPool.WorkerCount(), jobCount);
	for (size_t w = 1; w < extra; ++w)
		kd.collectFutures.push_back(kd.threadPool.SubmitJobWithFuture(worker));
	worker(); // calling thread helps too
	for (auto& fut : kd.collectFutures) fut.get();
}

void AbstractMIDIRenderer::EndNoteFrame(const NoteFrame& f)
{
	auto& nc = *noteCounterInfo;
	nc.notesPassed.value = (uint64_t)f.notesPassed;
	nc.polyphony.value = (uint64_t)f.polyphony;

	if (!nc.npsHistory.empty() && f.playbackSeconds < nc.npsHistory.back().timeSeconds)
		nc.npsHistory.clear();

	nc.npsHistory.push_back({ f.playbackSeconds, (uint64_t)f.notesPassed });
	while (!nc.npsHistory.empty() && (f.playbackSeconds - nc.npsHistory.front().timeSeconds) > 1.0)
		nc.npsHistory.pop_front();

	const uint64_t before = nc.npsHistory.empty() ? 0 : nc.npsHistory.front().totalNotes;
	nc.notesPerSecond.value = ((uint64_t)f.notesPassed >= before && !nc.npsHistory.empty())
		? (uint64_t)f.notesPassed - before : 0;
}

void AbstractMIDIRenderer::RenderSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();
	ImGui::Separator();

	{
		ForumLayout layout("##globalSettings");
		layout.AddField("##bgColor", "Background color",
			[&]()
			{
				ImVec4 bgColorVec = config->render.GetBackground();
				float bgColor[3]{ bgColorVec.x, bgColorVec.y, bgColorVec.z };
				if (ImGui::ColorEdit3("##bgColor", bgColor))
				{
					config->render.SetBackground(bgColor[0], bgColor[1], bgColor[2]);
				}
			});

		layout.AddField("##keyRange", "Key range",
			[&]()
			{
				const float tildeWidth = ImGui::CalcTextSize("~").x;
				const float availableWidth = ImGui::GetContentRegionAvail().x;
				if (availableWidth < 0.0) return;

				const float rangeWidth = (availableWidth - tildeWidth) / 2.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f;
				
				int keyFirst = config->render.GetKeyFirst();
				int keyLast = config->render.GetKeyLast();

				ImGui::SetNextItemWidth(rangeWidth);
				if (ImGui::InputInt("##first", &keyFirst, 0, 0))
				{
					config->render.SetKeyFirst(keyFirst);
				}

				ImGui::SameLine();
				ImGui::Text("~");
				ImGui::SameLine();

				ImGui::SetNextItemWidth(rangeWidth);
				if (ImGui::InputInt("##last", &keyLast, 0, 0))
				{
					config->render.SetKeyLast(keyLast);
				}

				std::string previewText;
				if (keyFirst == 21 && keyLast == 108)
					previewText = "88 Keys";
				else if (keyFirst == 0 && keyLast == 127)
					previewText = "128 Keys";
				else
					previewText = "Custom";

				if (ImGui::BeginCombo("##keyRangePreset", previewText.c_str()))
				{
					if (ImGui::Selectable("88 Keys", keyFirst == 21 && keyLast == 108))
					{
						config->render.SetKeyFirst(21);
						config->render.SetKeyLast(108);
					}

					if (ImGui::Selectable("128 Keys", keyFirst == 0 && keyLast == 127))
					{
						config->render.SetKeyFirst(0);
						config->render.SetKeyLast(127);
					}

					ImGui::EndCombo();
				}
			});

		{
			ForumLayoutBreak brk(&layout);
			ImGui::Separator();
		}

		static bool showAdvanced = false;
		layout.AddField("##showAdvanced", "Show advanced settings",
			[&]()
			{
				ImGui::Checkbox("", &showAdvanced);
			});

		if (showAdvanced)
		{
			layout.AddFieldWithTooltip("##marginPx", "Row margin (px)",
				"How much headroom to leave when collecting row owners per key. 0 = At most one note per pixel row, but can introduce rendering artifacts.",
				[&]()
				{
					float margin = advancedSettings.rowMarginPx;
					if (ImGui::InputFloat("", &margin, 0, 0))
					{
						if (margin < 0) margin = 0;
						if (margin > 0.02) margin = 0.02;
						advancedSettings.rowMarginPx = margin;
					}
				});
		}
	}
	
	if (ImGui::Button("Reset settings"))
	{
		advancedSettings = {};
		ResetSettings();
	}
}

void AbstractMIDIRenderer::OnResize(int newWidth, int newHeight)
{
	width = newWidth;
	height = newHeight;
	if (sceneFramebuffer)
	{
		sceneFramebuffer->Resize(width, height);
	}
}

GLuint AbstractMIDIRenderer::GetSceneTexture()
{
	return sceneFramebuffer ? sceneFramebuffer->GetSceneTexture() : 0;
}