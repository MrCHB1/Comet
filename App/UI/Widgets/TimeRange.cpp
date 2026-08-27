#include "TimeRange.h"
#include "Utils.h"
#include "imgui.h"
#include <string>
#include <optional>

bool TimeRange::Draw(const char* id, double* start, double* end, double min, double max)
{
	bool change = false;
	std::string idStr = id;

#define HOVER_TOOLTIP "Enter a timestamp (E.g. MM:SS, HH:MM:SS, MM:SS.m, SS)"

	static char buf[64];
	strncpy(buf, Utils::FormatDuration2(*start * 1000).c_str(), sizeof(buf));

	ImGui::SetNextItemWidth(100);
	if (ImGui::InputText((idStr + "_start").c_str(), buf, sizeof(buf)))
	{
		std::optional<double> parsed = Utils::ParseTimeString(std::string(buf));
		if (parsed)
		{
			double val = *parsed;
			if (val < min) val = min;
			*start = val;

			change = true;
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(HOVER_TOOLTIP);
	}

	ImGui::SameLine();
	ImGui::Text("-");
	ImGui::SameLine();

	strncpy(buf, Utils::FormatDuration2(*end * 1000).c_str(), sizeof(buf));
	ImGui::SetNextItemWidth(100);
	if (ImGui::InputText((idStr + "_end").c_str(), buf, sizeof(buf)))
	{
		std::optional<double> parsed = Utils::ParseTimeString(std::string(buf));
		if (parsed)
		{
			double val = *parsed;
			if (val > max) val = max;
			*end = val;

			change = true;
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(HOVER_TOOLTIP);
	}

#undef HOVER_TOOLTIP
	return change;
}