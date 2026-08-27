#include "StatsOverlayRenderer.h"
#include "App/Fonts.h"
#include "App/FontList.h"
#include "App/MIDIApp.h"
#include "Utils.h"

bool StatsOverlayRenderer::IsShown()
{
	if (app->IsRendering()) return false;

	MIDIPlayerConfig* config = app->GetConfig();
	return config->render.showStats;
}

void StatsOverlayRenderer::Render(float heightOffset)
{
	MIDIPlayerConfig* config = app->GetConfig();
	ImVec4 bgColor = config->overlayInfo.backgroundCol;
	ImVec4 txtColor = config->overlayInfo.textCol;

	NoteCounterAlignment counterAlignment = config->overlayInfo.overlayAlignment;
	NoteCounterStyle counterStyle = config->overlayInfo.overlayStyle;

	ImFont* fontToUse = nullptr;
	for (const auto& f : app->GetFontList()->GetFonts())
	{
		if (f.path == config->overlayInfo.selectedFontPath)
		{
			fontToUse = f.font;
			break;
		}
	}

	lastStatsYOffset = heightOffset;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		case NoteCounterAlignment::TopCenter:
		{
			ImGui::SetNextWindowPos(ImVec2((float)width, heightOffset), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			break;
		}
		case NoteCounterAlignment::TopRight:
		{
			ImGui::SetNextWindowPos(ImVec2(0, heightOffset), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
			break;
		}
	}

	if (fontToUse) ImGui::PushFont(fontToUse);
	else ImGui::PushFont(Fonts::MonoFont);

	float statsScale = config->overlayInfo.scale;
	float minStatsWidth = statsWidth * statsScale;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f * statsScale, 5.0f * statsScale));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
	ImGui::PushStyleColor(ImGuiCol_Text, txtColor);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-10.0f, 2.0f));

	// render debug stuff here
	if (ImGui::Begin("statsOverlay", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
	{
		ImGui::SetWindowFontScale(statsScale);

		struct StatsRow { const char* label; char value[64]; };
		StatsRow rows[2];
		int rowCount = 0;

		{
			OverlayUtils::FormatText(rows[rowCount].value, "%.1f", app->GetFPS());
			rows[rowCount].label = "FPS";
			rowCount++;

			double buffer = app->GetAudioBufferSeconds();
			OverlayUtils::FormatText(rows[rowCount].value, buffer < -0.5 ? "N/A" : Utils::FormatDuration2(buffer * 1000).c_str(), buffer);
			rows[rowCount].label = "Buffer";
			rowCount++;
		}

		float labelColWidth = 0.0f, valueColWidth = 0.0f;
		for (int i = 0; i < rowCount; i++)
		{
			labelColWidth = std::max(labelColWidth, ImGui::CalcTextSize(rows[i].label).x);
			valueColWidth = std::max(valueColWidth, ImGui::CalcTextSize(rows[i].value).x);
		}

		float windowPaddingX = 5.0f * statsScale;
		float cellPaddingX = ImGui::GetStyle().CellPadding.x;
		float naturalContentWidth = labelColWidth + valueColWidth + cellPaddingX * 4.0f;
		float minContentWidth = std::max(0.0f, minStatsWidth - windowPaddingX * 2.0f);
		float tableWidth = std::max(minContentWidth, naturalContentWidth);

		if (ImGui::BeginTable("statsOverlayTb", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoClip, ImVec2(tableWidth, 0)))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoClip);

			for (int i = 0; i < rowCount; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(rows[i].label);
				ImGui::TableSetColumnIndex(1);
				OverlayUtils::RightAlignedTableText(rows[i].value);
			}

			lastStatsWidth = ImGui::GetWindowWidth();
			lastStatsHeight = ImGui::GetWindowHeight();

			ImGui::EndTable();
		}
		ImGui::SetWindowFontScale(1.0f);
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
	ImGui::PopFont();
}

void StatsOverlayRenderer::OnResize(int width, int height)
{
	this->width = width;
	this->height = height;
}

glm::vec2 StatsOverlayRenderer::GetOverlayPosition() const
{
	MIDIPlayerConfig* config = app->GetConfig();
	NoteCounterAlignment counterAlignment = config->overlayInfo.overlayAlignment;

	float width = (float)lastStatsWidth / (float)this->width;
	float height = (float)lastStatsHeight / (float)this->height;

	float yPos = 1.0 - height - lastStatsYOffset / (float)this->height;
	switch (counterAlignment)
	{
		case NoteCounterAlignment::TopLeft:
		case NoteCounterAlignment::TopCenter:
		{
			return glm::vec2(1.0 - width, yPos);
		}
		case NoteCounterAlignment::TopRight:
		{
			return glm::vec2(0.0, yPos);
		}
		default:
			return glm::vec2(0.0f);
	}
}

glm::vec2 StatsOverlayRenderer::GetOverlaySize() const
{
	float width = lastStatsWidth / (float)this->width;
	float height = lastStatsHeight / (float)this->height;
	return glm::vec2(width, height);
}