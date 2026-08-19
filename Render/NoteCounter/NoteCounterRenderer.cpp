#include "NoteCounterRenderer.h"
#include "App/Fonts.h"
#include "App/FontList.h"
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
	MIDIPlayerConfig* config = app->GetConfig();

	// font magic
	ImFont* fontToUse = nullptr;
	for (const auto& f : app->GetFontList()->GetFonts())
	{
		if (f.path == config->overlayInfo.selectedFontPath)
		{
			fontToUse = f.font;
			break;
		}
	}

	lastCounterYOffset = heightOffset;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		{
			ImGui::SetNextWindowPos(ImVec2(0, heightOffset), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
			break;
		}
		case NoteCounterAlignment::TopCenter:
		{
			ImGui::SetNextWindowPos(ImVec2((float)width * 0.5f, heightOffset), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
			break;
		}
		case NoteCounterAlignment::TopRight:
		{
			ImGui::SetNextWindowPos(ImVec2((float)width, heightOffset), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			break;
		}
	}

	if (fontToUse) ImGui::PushFont(fontToUse);
	else ImGui::PushFont(Fonts::MonoFont);

	float counterScale = config->overlayInfo.scale;
	float minCounterWidth = this->counterWidth * counterScale;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f * counterScale, 5.0f * counterScale));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, noteCounterBackgroundCol);
	ImGui::PushStyleColor(ImGuiCol_Text, noteCounterTextCol);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-10.0f, 2.0f));

	
	switch (counterStyle)
	{
		case NoteCounterStyle::UMP:
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(minCounterWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
			RenderUMP(counterScale);
			break;
		}
		case NoteCounterStyle::MIDITrail:
			RenderMIDITrail(counterScale);
			break;
		default: break;
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
	ImGui::PopFont();
}

void NoteCounterRenderer::RenderUMP(float counterScale)
{
	float minCounterWidth = this->counterWidth * counterScale;

	if (ImGui::Begin("noteCounterUMP", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::SetWindowFontScale(counterScale);

		// unfortunate that i have to completely change how i calculate counter width, else it fucks with the overall counter width
		// hence why it was wider than usual when the scake is 1.0 :/

		struct CounterRow { const char* label; char value[64]; };
		CounterRow rows[8];
		int rowCount = 0;

		if (noteCounterInfo->tick.shown)
		{
			auto ticks = noteCounterInfo->tick.value;
			FormatText(rows[rowCount].value, "%s/%u", Utils::FormatWithCommas(ticks > 0 ? ticks : 0).c_str(), noteCounterInfo->ppq.value);
			rows[rowCount].label = "Tick";
			rowCount++;
		}
		if (noteCounterInfo->timeSeconds.shown)
		{
			FormatText(rows[rowCount].value, "%s", Utils::FormatDuration2(noteCounterInfo->timeSeconds.value * 1000).c_str());
			rows[rowCount].label = "Time";
			rowCount++;
		}
		if (noteCounterInfo->bpm.shown)
		{
			FormatText(rows[rowCount].value, "%.1f", noteCounterInfo->bpm.value);
			rows[rowCount].label = "BPM";
			rowCount++;
		}
		if (noteCounterInfo->notesPassed.shown)
		{
			FormatText(rows[rowCount].value, "%s", Utils::FormatWithCommas(noteCounterInfo->notesPassed.value).c_str());
			rows[rowCount].label = "Notes";
			rowCount++;
		}
		if (noteCounterInfo->notesPerSecond.shown)
		{
			FormatText(rows[rowCount].value, "%s", Utils::FormatWithCommas(noteCounterInfo->notesPerSecond.value).c_str());
			rows[rowCount].label = "NPS";
			rowCount++;
		}
		if (noteCounterInfo->polyphony.shown)
		{
			FormatText(rows[rowCount].value, "%s", Utils::FormatWithCommas(noteCounterInfo->polyphony.value).c_str());
			rows[rowCount].label = "Polyphony";
			rowCount++;
		}
		if (!app->IsRendering())
		{
			if (noteCounterInfo->fps.shown)
			{
				FormatText(rows[rowCount].value, "%.1f", noteCounterInfo->fps.value);
				rows[rowCount].label = "FPS";
				rowCount++;
			}

			if (noteCounterInfo->audioBuffer.shown)
			{
				double buffer = noteCounterInfo->audioBuffer.value;
				FormatText(rows[rowCount].value, buffer < -0.5 ? "N/A" : Utils::FormatDuration2(buffer * 1000).c_str(), buffer);
				rows[rowCount].label = "Buffer";
				rowCount++;
			}
		}
		

		float labelColWidth = 0.0f, valueColWidth = 0.0f;
		for (int i = 0; i < rowCount; i++)
		{
			labelColWidth = std::max(labelColWidth, ImGui::CalcTextSize(rows[i].label).x);
			valueColWidth = std::max(valueColWidth, ImGui::CalcTextSize(rows[i].value).x);
		}

		float windowPaddingX = 5.0f * counterScale;
		float cellPaddingX = ImGui::GetStyle().CellPadding.x; // matches the ImGuiStyleVar_CellPadding pushed in Render()
		float naturalContentWidth = labelColWidth + valueColWidth + cellPaddingX * 4.0f;
		float minContentWidth = std::max(0.0f, minCounterWidth - windowPaddingX * 2.0f);
		float tableWidth = std::max(minContentWidth, naturalContentWidth);

		if (ImGui::BeginTable("counterStats", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoClip, ImVec2(tableWidth, 0.0f)))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoClip);

			for (int i = 0; i < rowCount; i++)
			{
				BeginNextCounterRow(rows[i].label);
				RightAlignedTableText(rows[i].value);
			}

			lastCounterWidth = ImGui::GetWindowWidth();
			lastCounterHeight = ImGui::GetWindowHeight();

			ImGui::EndTable();
		}
		ImGui::SetWindowFontScale(1.0f);
	}
	ImGui::End();
}

void NoteCounterRenderer::RenderMIDITrail(float counterScale)
{
	if (ImGui::Begin("noteCounterMIDITrail", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::SetWindowFontScale(counterScale);

		std::stringstream stats;
		
		bool hasFirstField = false;

		if (noteCounterInfo->tick.shown)
		{
			stats << "TICK:" << Utils::FormatWithCommas(noteCounterInfo->tick.value) << "/" << noteCounterInfo->ppq.value;
			hasFirstField = true;
		}

		if (noteCounterInfo->timeSeconds.shown)
		{
			if (hasFirstField) stats << "  ";
			stats << "TIME:" << Utils::FormatDuration2(noteCounterInfo->timeSeconds.value * 1000);
			hasFirstField = true;
		}

		if (noteCounterInfo->bpm.shown)
		{
			if (hasFirstField) stats << "  ";
			stats << "BPM:" << std::fixed << std::setprecision(1) << noteCounterInfo->bpm.value;
			hasFirstField = true;
		}

		if (noteCounterInfo->notesPassed.shown)
		{
			if (hasFirstField) stats << "  ";
			stats << "NOTES:" << Utils::FormatWithCommas(noteCounterInfo->notesPassed.value);
			hasFirstField = true;
		}

		if (noteCounterInfo->notesPerSecond.shown)
		{
			if (hasFirstField) stats << "  ";
			stats << "NPS:" << Utils::FormatWithCommas(noteCounterInfo->notesPerSecond.value);
			hasFirstField = true;
		}

		if (noteCounterInfo->polyphony.shown)
		{
			if (hasFirstField) stats << "  ";
			stats << "POLY:" << Utils::FormatWithCommas(noteCounterInfo->polyphony.value);
			hasFirstField = true;
		}

		if (noteCounterInfo->fps.shown && !app->IsRendering())
		{
			if (hasFirstField) stats << "  ";
			stats << "FPS:" << std::fixed << std::setprecision(1) << noteCounterInfo->fps.value;
		}

		ImGui::Text(stats.str().c_str());

		lastCounterWidth = ImGui::GetWindowWidth();
		lastCounterHeight = ImGui::GetWindowHeight();

		ImGui::SetWindowFontScale(1.0f);
	}
	ImGui::End();
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

	float yPos = 1.0 - height - lastCounterYOffset / (float)this->height;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		{
			return glm::vec2(0.0, yPos);
		}
		case NoteCounterAlignment::TopCenter:
		{
			return glm::vec2(0.5f - width * 0.5f, yPos);
		}
		case NoteCounterAlignment::TopRight:
		{
			return glm::vec2(1.0 - width, yPos);
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