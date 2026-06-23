#include "NoteCounterRenderer.h"
#include "App/Fonts.h"
#include "imgui.h"
#include <string>
#include "Utils.h"

static void RightAlignedTableText(const char* text)
{
	float textWidth = ImGui::CalcTextSize(text).x;
	float avail = ImGui::GetContentRegionAvail().x;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textWidth);
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
	float counterScale = config->overlayInfo.scale;
	float counterWidth = this->counterWidth * counterScale;

	lastCounterYOffset = heightOffset;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		{
			ImGui::SetNextWindowPos(ImVec2(0, heightOffset));
			break;
		}
		case NoteCounterAlignment::TopRight:
		{
			ImGui::SetNextWindowPos(ImVec2(width - counterWidth, heightOffset));
			break;
		}
	}
	
	ImGui::SetNextWindowSize(ImVec2(counterWidth, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f * counterScale, 5.0f * counterScale));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, noteCounterBackgroundCol);
	ImGui::PushStyleColor(ImGuiCol_Text, noteCounterTextCol);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::Begin("noteCounter", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
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
				FormatText(buf, "%s/%u", Utils::FormatWithCommas(ticks > 0 ? ticks : 0).c_str(), noteCounterInfo->ppq.value);
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
			
			ImGui::PopFont();

			lastCounterHeight = ImGui::GetWindowHeight();

			ImGui::EndTable();
		}
		ImGui::SetWindowFontScale(1.0f);
		ImGui::End();
	}
	ImGui::PopStyleVar(2);
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