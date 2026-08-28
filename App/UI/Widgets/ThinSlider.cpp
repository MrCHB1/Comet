#include "ThinSlider.h"
#include "imgui_internal.h"
#include <algorithm>
#include <type_traits>

template <typename T>
bool ThinSlider::Draw(
	const char* id,
	T* v,
	T min,
	T max,
	const char* format,
	ImGuiSliderFlags flags,
	float heightPercent)
{
#pragma region Type validity
	static_assert(std::is_same<T, int>::value || std::is_same<T, float>::value);

	ImGuiDataType dataType;
	if constexpr (std::is_same_v<T, float>)
		dataType = ImGuiDataType_Float;
	else
		dataType = ImGuiDataType_S32;
#pragma endregion

	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;

	if (window->SkipItems)
		return false;

	heightPercent = std::clamp(heightPercent, 0.0f, 1.0f);

	const ImGuiStyle& style = g.Style;

	const float width = ImGui::CalcItemWidth();
	const float height = ImGui::GetFrameHeight();

	const ImVec2 pos = window->DC.CursorPos;
	const ImRect bb(
		pos,
		ImVec2(pos.x + width, pos.y + height)
	);

	const bool tempInputAllowed = (flags & ImGuiSliderFlags_NoInput) == 0;
	ImGuiItemFlags itemFlags = tempInputAllowed ? ImGuiItemFlags_Inputable : ImGuiItemFlags_None;

	const ImGuiID idHash = window->GetID(id);
	ImGui::ItemSize(bb, style.FramePadding.y);

	if (!ImGui::ItemAdd(bb, idHash, &bb, itemFlags))
		return false;

	const bool hovered = ImGui::ItemHoverable(
		bb,
		idHash,
		ImGuiItemFlags_None
	);

	bool tempInputIsActive = tempInputAllowed && g.ActiveId == idHash && g.TempInputId == idHash;

	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, 0, idHash))
	{
		ImGui::SetKeyOwner(ImGuiKey_MouseLeft, idHash);

		if (tempInputAllowed && g.IO.KeyCtrl)
		{
			tempInputIsActive = true;
			ImGui::SetActiveID(idHash, window);
			ImGui::SetFocusID(idHash, window);
			ImGui::FocusWindow(window);

			g.ActiveIdUsingNavDirMask = 0;
		}
		else
		{
			ImGui::SetActiveID(idHash, window);
			ImGui::SetFocusID(idHash, window);
			ImGui::FocusWindow(window);
		}
	}

	ImRect grabBB;
	bool valueChanged = false;

	if (tempInputIsActive)
	{
		valueChanged = ImGui::TempInputScalar(
			bb,
			idHash,
			id,
			dataType,
			v,
			format,
			&min,
			&max
		);

		if (valueChanged)
			ImGui::MarkItemEdited(idHash);
	}
	else
	{
		valueChanged = ImGui::SliderBehavior(
			bb,
			idHash,
			dataType,
			v,
			&min,
			&max,
			format,
			flags,
			&grabBB
		);

		if (valueChanged)
			ImGui::MarkItemEdited(idHash);
	}

#pragma region Thin slider rendering
	ImDrawList* drawList = window->DrawList;

	const float trackHeight = height * heightPercent;
	const float trackY = bb.Min.y + (height - trackHeight) * 0.5f;

	const ImGuiCol trackCol =
		hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg;

	
	if (!tempInputIsActive)
	{
		drawList->AddRectFilled(
			ImVec2(bb.Min.x, trackY),
			ImVec2(bb.Max.x, trackY + trackHeight),
			ImGui::GetColorU32(trackCol),
			trackHeight * 0.5f
		);

		if (grabBB.Max.x > grabBB.Min.x)
		{
			const bool active = g.ActiveId == idHash;
			const ImGuiCol grabColor =
				active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab;

			drawList->AddRectFilled(
				grabBB.Min,
				grabBB.Max,
				ImGui::GetColorU32(grabColor),
				style.GrabRounding
			);
		}
	}
#pragma endregion

	return valueChanged;
}

template bool ThinSlider::Draw(const char* id, float* v, float min, float max, const char* format, ImGuiSliderFlags flags, float heightPercent);
template bool ThinSlider::Draw(const char* id, int* v, int min, int max, const char* format, ImGuiSliderFlags flags, float heightPercent);