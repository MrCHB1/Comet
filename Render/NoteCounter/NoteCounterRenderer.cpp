#include "NoteCounterRenderer.h"
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
	lastCounterYOffset = heightOffset;
	ImGui::SetNextWindowPos(ImVec2(width - counterWidth, heightOffset));
	ImGui::SetNextWindowSize(ImVec2(counterWidth, 0.0f));
	// ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::Begin("noteCounter", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
	{
		if (ImGui::BeginTable("counterStats", 2))
		{
			char buf[64];

			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			// these can be toggled, therefore height calculation may be tricky
			BeginNextCounterRow("Tick");
			auto ticks = noteCounterInfo->tick.value;
			FormatText(buf, "%s/%u", Utils::FormatWithCommas(ticks > 0 ? ticks : 0).c_str(), noteCounterInfo->ppq.value);
			RightAlignedTableText(buf);

			BeginNextCounterRow("Time");
			FormatText(buf, "%s", Utils::FormatDuration2(noteCounterInfo->timeSeconds.value * 1000).c_str());
			RightAlignedTableText(buf);

			BeginNextCounterRow("BPM");
			FormatText(buf, "%.1f", noteCounterInfo->bpm.value);
			RightAlignedTableText(buf);

			BeginNextCounterRow("Notes");
			FormatText(buf, "%s", Utils::FormatWithCommas(noteCounterInfo->notesPassed.value).c_str());
			RightAlignedTableText(buf);

			BeginNextCounterRow("Polyphony");
			FormatText(buf, "%s", Utils::FormatWithCommas(noteCounterInfo->polyphony.value).c_str());
			RightAlignedTableText(buf);

			lastCounterHeight = ImGui::GetWindowHeight();

			ImGui::EndTable();
		}
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