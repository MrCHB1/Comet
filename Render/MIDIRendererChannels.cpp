#include "MIDIRendererChannels.h"
#include "App/MIDIApp.h"
#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"

static const char* channelsVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in int aKeyChannel;
layout (location = 2) in uint aMeta;

uniform int totalKeys;
uniform int totalChannels;

flat out uint meta;

void main()
{
	int key = aKeyChannel & 0x7F;
	int channel = (aKeyChannel >> 7) & 0xF;
	float invKeys = 1.0f / float(totalKeys);
	float invChannels = 1.0f / float(totalChannels);

	float left = float(key) * invKeys;
	float right = float(key + 1) * invKeys;

	int topDownChannel = totalChannels - channel;
	float bottom = float(topDownChannel - 1) * invChannels;
	float top = float(topDownChannel) * invChannels;

	float x = mix(left, right, aPos.x);
	float y = mix(bottom, top, aPos.y);
	gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);

	meta = aMeta;
})";

static const char* channelsFrag = R"(#version 330 core

flat in uint meta;

out vec4 fragColor;

void main()
{
	vec3 color = vec3(
		float((meta >> 16u) & 0xFFu) / 255.0,
		float((meta >> 8u) & 0xFFu) / 255.0,
		float(meta & 0xFFu) / 255.0
	);

	fragColor = vec4(color, 1.0);
})";

void MIDIRendererChannels::Initialize()
{
	AbstractMIDIRenderer::Initialize();
	channelProgram = ShaderProgram::Create(channelsVert, channelsFrag);
	channelVAO = std::make_unique<VertexArray>();
	channelVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	channelIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	channelEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*channelVAO);

		std::array<float, 8> quadVertices{
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f
		};

		std::array<int, 6> quadIndices{
			0, 1, 3,
			1, 2, 3
		};
		channelVBO->Bind();
		channelVBO->SetData(quadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), (void*)0);

		channelEBO->Bind();
		channelEBO->SetData(quadIndices, GL_STATIC_DRAW);

		channelIBO->Bind();
		channelIBO->SetData(renderChannelKeys, GL_DYNAMIC_DRAW);

		channelVAO->SetIntAttribute(1, 1, sizeof(RenderChannelKey), offsetof(RenderChannelKey, keyChannel));
		channelVAO->SetIntAttribute(2, 1, sizeof(RenderChannelKey), offsetof(RenderChannelKey, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
	}

	{
		ShaderBind shader(*channelProgram);

		channelProgram->SetInt("totalKeys", MIDI_KEYS);
		channelProgram->SetInt("totalChannels", 16);
	}
}

void MIDIRendererChannels::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
	std::lock_guard<std::mutex> guard(renderMutex);

	if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
	AbstractMIDIRenderer::LoadSequence(sequence);

	colors.LoadColors();
	seq = sequence;

	lastTime = 0;

	for (auto& id : startRenderIDs)
		id = 0;
}

void MIDIRendererChannels::UnloadSequence()
{
	std::lock_guard<std::mutex> guard(renderMutex);
	AbstractMIDIRenderer::UnloadSequence();
}

void MIDIRendererChannels::Render(double deltaTime)
{
	sceneFramebuffer->Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	RenderChannelKeys();
	sceneFramebuffer->Unbind();
}

void MIDIRendererChannels::RenderChannelKeys()
{
	if (!seq) return;
	std::vector<NoteSequence>& notes = seq->mergedNotes;
	if (notes.empty()) return;

	double playbackSeconds = app->GetTimer()->Elapsed();

	TempoMap* tempoMap = seq->GetTempoMap();
	long time = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);
	double bpm = tempoMap->GetBPMAtTick(time);
	noteCounterInfo->tick = time >= 0 ? time : 0;
	noteCounterInfo->timeSeconds = playbackSeconds;
	noteCounterInfo->bpm = bpm;

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	size_t notesPassed = 0;
	size_t polyphony = 0;

	for (uint8_t key = 0; key < MIDI_KEYS; key++)
	{
		NoteSequence& notesNote = notes[key];

		size_t noteBegin = startRenderIDs[key];

		// we still must incorporate culling logic if we want the renderer to be efficient
		if (lastTime < time)
		{
			while (noteBegin < notesNote.Size())
			{
				double noteEnd = isTimeBased
					? (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]) * invTimeMultiplier
					: (double)(notesNote.tick[noteBegin] + notesNote.gate[noteBegin]);

				if (noteEnd > accTime) break; // Note is still on screen
				++noteBegin;
			}
		}
		else if (lastTime > time)
		{
			while (noteBegin > 0)
			{
				size_t prev = noteBegin - 1;
				double noteEnd = isTimeBased
					? (double)(notesNote.tick[prev] + notesNote.gate[prev]) * invTimeMultiplier
					: (double)(notesNote.tick[prev] + notesNote.gate[prev]);

				if (noteEnd <= accTime) break;
				--noteBegin;
			}
		}
		
		startRenderIDs[key] = noteBegin;
		notesPassed += noteBegin;

		for (size_t i = noteBegin; i < notesNote.Size(); ++i)
		{
			uint32_t nTick = notesNote.tick[i];
			uint32_t nGate = notesNote.gate[i];
			uint8_t nNote = notesNote.note[i];
			uint16_t nTrack = notesNote.track[i];
			uint8_t nChannel = notesNote.channel[i];

			double noteStart = isTimeBased ? (double)nTick * invTimeMultiplier : (double)nTick;
			double noteEnd = isTimeBased
				? (double)(nTick + nGate) * invTimeMultiplier
				: (double)(nTick + nGate);

			if (noteEnd <= accTime)
			{
				notesPassed++;
				continue;
			}

			if (noteStart > accTime) break; // no need to iterate further

			size_t index = nNote + MIDI_KEYS * nChannel;
			channelKeyMetas[index].MarkActive(true);
			channelKeyMetas[index].color = colors.GetColor(nTrack, nChannel);
			channelKeyMetas[index].SetKey(nNote);
			channelKeyMetas[index].SetChannel(nChannel);

			notesPassed++;
			polyphony++;
		}
	}

	// Render!
	size_t numActive = 0;
	for (const auto& meta : channelKeyMetas)
	{
		if (!meta.active) continue;
		auto& chKey = renderChannelKeys[numActive];
		chKey.keyChannel = static_cast<int>(meta.keyChannel);
		chKey.meta = meta.GetMeta();
		numActive++;
	}

	if (numActive == 0) return;

	ShaderBind shader(*channelProgram);

	VertexArrayBind vaoBind(*channelVAO);
	BufferBind vboBind(*channelVBO);
	BufferBind eboBind(*channelEBO);
	BufferBind iboBind(*channelIBO);

	glBufferSubData(GL_ARRAY_BUFFER, 0, numActive * sizeof(RenderChannelKey), renderChannelKeys.data());
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, numActive);

	// reset the key activeness

	for (auto& meta : channelKeyMetas)
	{
		meta.MarkActive(false);
	}

	// update the info

	noteCounterInfo->notesPassed = static_cast<uint64_t>(notesPassed);
	noteCounterInfo->polyphony = static_cast<uint64_t>(polyphony);

	if (!noteCounterInfo->npsHistory.empty() && playbackSeconds < noteCounterInfo->npsHistory.back().timeSeconds)
	{
		noteCounterInfo->npsHistory.clear();
	}

	noteCounterInfo->npsHistory.push_back({ playbackSeconds, static_cast<uint64_t>(notesPassed) });
	while (!noteCounterInfo->npsHistory.empty() &&
		(playbackSeconds - noteCounterInfo->npsHistory.front().timeSeconds) > 1.0)
	{
		noteCounterInfo->npsHistory.pop_front();
	}

	if (!noteCounterInfo->npsHistory.empty())
	{
		uint64_t notesOneSecondAgo = noteCounterInfo->npsHistory.front().totalNotes;
		uint64_t currentNotes = static_cast<uint64_t>(notesPassed);

		if (currentNotes >= notesOneSecondAgo)
		{
			noteCounterInfo->notesPerSecond.value = currentNotes - notesOneSecondAgo;
		}
		else
		{
			noteCounterInfo->notesPerSecond.value = 0;
		}
	}
	else
	{
		noteCounterInfo->notesPerSecond = 0;
	}

	lastTime = time;
}