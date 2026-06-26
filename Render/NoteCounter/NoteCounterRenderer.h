#pragma once

#include "NoteCounterInfo.h"
#include "NoteCounterStyles.h"
#include "Config/MIDIPlayerConfig.h"
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include "imgui.h"

class MIDIApp;

class NoteCounterRenderer
{
public:
	NoteCounterRenderer(std::shared_ptr<NoteCounterInfo> noteCounterInfo, MIDIApp* app) : noteCounterInfo(noteCounterInfo), app(app) {}
	// we use heightOffset here because of the nagivation bar. when rendering a video, the navigation bar is hidden, so the counter should be rendered higher up to compensate for that. when not rendering a video, the navigation bar is visible, so the counter should be rendered lower down to avoid overlapping with it.
	void Render(float heightOffset);
	void OnResize(int width, int height);

	const NoteCounterAlignment& GetCounterAlignment() const
	{
		return counterAlignment;
	}
	void SetCounterAlignment(NoteCounterAlignment alignment)
	{
		counterAlignment = alignment;
	}

	const std::array<float, 4> GetCounterBackground() const
	{
		const ImVec4& bgCol = noteCounterBackgroundCol;
		return { bgCol.x, bgCol.y, bgCol.z, bgCol.w };
	}
	void SetCounterBackground(float r, float g, float b, float a)
	{
		noteCounterBackgroundCol.x = r;
		noteCounterBackgroundCol.y = g;
		noteCounterBackgroundCol.z = b;
		noteCounterBackgroundCol.w = a;
	}

	const std::array<float, 3> GetCounterTextColor() const
	{
		const ImVec4& textCol = noteCounterTextCol;
		return { textCol.x, textCol.y, textCol.z };
	}
	void SetCounterTextColor(float r, float g, float b)
	{
		noteCounterTextCol.x = r;
		noteCounterTextCol.y = g;
		noteCounterTextCol.z = b;
	}
	
	glm::vec2 GetCounterPosition() const;
	glm::vec2 GetCounterResolution() const;
	float GetCounterHeight() const;
private:
	std::shared_ptr<NoteCounterInfo> noteCounterInfo;
	NoteCounterStyle counterStyle = DEFAULT_NOTE_COUNTER_STYLE;
	NoteCounterAlignment counterAlignment = DEFAULT_NOTE_COUNTER_ALIGNMENT;
	ImVec4 noteCounterBackgroundCol = ImVec4(0.f, 0.f, 0.f, 0.6f);
	ImVec4 noteCounterTextCol = ImVec4(1.f, 1.f, 1.f, 1.f);
	MIDIApp* app;

	int width = 0, height = 0;
	int counterWidth = DEFAULT_NOTE_COUNTER_WIDTH;
	float lastCounterYOffset = 0.0f;
	float lastCounterHeight = 0.0f;
};