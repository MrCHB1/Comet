#include <imgui.h>
#include "ForumLayout.h"

namespace Comet::UI::Layout
{

ForumLayout::ForumLayout(const char* id, float labelWidth)
	: tableId(id), labelWidth(labelWidth) { }

ForumLayout::~ForumLayout()
{
	if (!hasTable) return;
	ImGui::EndTable();
}

void ForumLayout::AddHeader(const char* text, float size)
{
	if (hasTable) ImGui::EndTable();

	ImGui::SetWindowFontScale(size);
	ImGui::TextUnformatted(text);
	ImGui::SetWindowFontScale(1.0f);
	ImGui::Separator();
	ImGui::Spacing();

	if (hasTable) BeginTable();
}

void ForumLayout::AddLargeHeader(const char* text)
{
	AddHeader(text, 1.5f);
}

void ForumLayout::BeginTable()
{
	hasTable = ImGui::BeginTable(std::format("{}##{}", tableId, tableNum++).c_str(),
		2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX);

	if (!hasTable) return;

	ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
	ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
}

}