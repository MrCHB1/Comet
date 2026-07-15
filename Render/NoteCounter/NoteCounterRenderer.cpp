#include "NoteCounterRenderer.h"
#include "App/Fonts.h"
#include "imgui.h"
#include <string>
#include "Utils.h"
#include "App/MIDIApp.h"

static void RightAlignedTableText(const char* text)
{
	float textWidth = ImGui::CalcTextSize(text).x;
	float avail = ImGui::GetContentRegionAvail().x;

	if (avail > textWidth)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textWidth);
	}

	ImGui::TextUnformatted(text);
}

template <size_t N, typename... Args>
static void FormatText(char (&buf)[N],
	const char* format, Args&&... args)
{
	std::snprintf(
		buf,
		N,
		format,
		std::forward<Args>(args)...
	);
}

static void BeginNextCounterRow(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(label);
	ImGui::TableSetColumnIndex(1);
}

void NoteCounterRenderer::Render(float heightOffset)
{
	auto* config = app->GetConfig();
	float counterScale = config->overlayInfo.scale;
	float minCounterWidth = this->counterWidth * counterScale;

	lastCounterYOffset = heightOffset;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		{
			ImGui::SetNextWindowPos(ImVec2(0, heightOffset), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
			break;
		}
		case NoteCounterAlignment::TopRight:
		{
			ImGui::SetNextWindowPos(ImVec2((float)width, heightOffset), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			break;
		}
	}
	
	ImGui::SetNextWindowSizeConstraints(ImVec2(minCounterWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f * counterScale, 5.0f * counterScale));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, noteCounterBackgroundCol);
	ImGui::PushStyleColor(ImGuiCol_Text, noteCounterTextCol);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-10.0f, 2.0f));

	if (ImGui::Begin("noteCounter", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::SetWindowFontScale(counterScale);
		if (ImGui::BeginTable("counterStats", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoClip))
		{
			char buf[64];

			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoClip);

			ImGui::PushFont(Fonts::MonoFont);

			// height calculation may be tricky lol
			if (noteCounterInfo->tick.shown)
			{
				BeginNextCounterRow("Tick");
				auto ticks = noteCounterInfo->tick.value;
				FormatText(buf, "%s/%u", Utils::FormatWithCommas(ticks > 0 ? ticks * 10001 : 0).c_str(), noteCounterInfo->ppq.value);
				RightAlignedTableText(buf);
			}

			if (noteCounterInfo->timeSeconds.shown)
			{
				BeginNextCounterRow("Time");
				FormatText(buf, "%s", Utils::FormatDuration2(noteCounterInfo->timeSeconds.value * 1000).c_str());
				RightAlignedTableText(buf);
			}

			if (noteCounterInfo->bpm.shown)
			{
				BeginNextCounterRow("BPM");
				FormatText(buf, "%.1f", noteCounterInfo->bpm.value);
				RightAlignedTableText(buf);
			}
			
			if (noteCounterInfo->notesPassed.shown)
			{
				BeginNextCounterRow("Notes");
				FormatText(buf, "%s", Utils::FormatWithCommas(noteCounterInfo->notesPassed.value).c_str());
				RightAlignedTableText(buf);
			}
			
			if (noteCounterInfo->notesPerSecond.shown)
			{
				BeginNextCounterRow("NPS");
				FormatText(buf, "%s", Utils::FormatWithCommas(noteCounterInfo->notesPerSecond.value).c_str());
				RightAlignedTableText(buf);
			}
			
			if (noteCounterInfo->polyphony.shown)
			{
				BeginNextCounterRow("Polyphony");
				FormatText(buf, "%s", Utils::FormatWithCommas(noteCounterInfo->polyphony.value).c_str());
				RightAlignedTableText(buf);
			}

			if (noteCounterInfo->fps.shown && !app->IsRendering())
			{
				BeginNextCounterRow("FPS");
				FormatText(buf, "%.1f", noteCounterInfo->fps.value);
				RightAlignedTableText(buf);
			}
			
			ImGui::PopFont();

			lastCounterWidth = ImGui::GetWindowWidth();
			lastCounterHeight = ImGui::GetWindowHeight();

			ImGui::EndTable();
		}
		ImGui::SetWindowFontScale(1.0f);
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
}

void NoteCounterRenderer::OnResize(int width, int height)
{
	this->width = width;
	this->height = height;
}

float NoteCounterRenderer::GetCounterHeight() const
{
	return lastCounterHeight;
}

glm::vec2 NoteCounterRenderer::GetCounterPosition() const
{
	float width = (float)lastCounterWidth / (float)this->width;
	float height = GetCounterHeight() / (float)this->height;

	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		{
			return glm::vec2(0.0, 1.0 - height - lastCounterYOffset / (float)this->height);
		}
		case NoteCounterAlignment::TopRight:
		{
			return glm::vec2(1.0 - width, 1.0 - height - lastCounterYOffset / (float)this->height);
		}
		default:
			return glm::vec2(0.0f);
	}
}

glm::vec2 NoteCounterRenderer::GetCounterResolution() const
{
	float width = lastCounterWidth / (float)this->width;
	float height = GetCounterHeight() / (float)this->height;
	return glm::vec2(width, height);
}