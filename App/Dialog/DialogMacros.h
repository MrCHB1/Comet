#pragma once

#include "imgui.h"

#define BEGIN_SECTION(id) \
	if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX))
#define END_SECTION \
	ImGui::EndTable()

#define SETUP_SECTION \
	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 160.0f); \
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

#define SECTION_ENTRY(a, ...) \
	do { ImGui::TableNextRow();\
		ImGui::TableSetColumnIndex(0);\
		a; \
		ImGui::TableSetColumnIndex(1); \
		__VA_ARGS__ \
	} while (0)

#define SECTION_LABEL(text) \
	ImGui::AlignTextToFramePadding(); \
	ImGui::TextUnformatted(text)

#define TABLE_LABEL_TOOLTIP(text, tooltip) \
	ImGui::AlignTextToFramePadding(); \
	ImGui::TextUnformatted(text); \
	ImGui::SetItemTooltip(tooltip)

#define SECTION_HEADER_LARGE(title) ImGui::Spacing(); \
	ImGui::SetWindowFontScale(1.5f); \
	ImGui::TextUnformatted(title); \
	ImGui::SetWindowFontScale(1.0f); \
	ImGui::Separator(); \
	ImGui::Spacing()

#define SECTION_HEADER(title) ImGui::Spacing(); \
	ImGui::SetWindowFontScale(1.2f); \
	ImGui::TextUnformatted(title); \
	ImGui::SetWindowFontScale(1.0f); \
	ImGui::Separator(); \
	ImGui::Spacing()

#define IMGUI_RADIO_BUTTON(label, variable, value) \
    if (ImGui::RadioButton(label, variable == value)) variable = value