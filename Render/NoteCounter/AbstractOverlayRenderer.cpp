#include "AbstractOverlayRenderer.h"
#include "imgui.h"

void OverlayUtils::RightAlignedTableText(const char* text)
{
	float textWidth = ImGui::CalcTextSize(text).x;
	float avail = ImGui::GetContentRegionAvail().x;

	if (avail > textWidth)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textWidth);
	}

	ImGui::TextUnformatted(text);
}