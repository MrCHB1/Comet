#include "NavigationBar.h"
#include "imgui.h"
#include <algorithm>

// TODO: Implement reset when unloading midi

void NavigationBar::Draw()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 2));
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, ImGui::GetFrameHeight()));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::Begin("MainDockWindow", nullptr, flags);
	ImGui::PopStyleVar();

	ImGui::SetNextItemWidth(viewport->WorkSize.x - 200);

	float currTimeSecs = timer->Elapsed();
	ImGui::PushID("nav_midiTime");
	if (ImGui::SliderFloat("", &currTimeSecs, -3.0f, midiLength, "", ImGuiSliderFlags_NoInput))
	{
		if (currTimeSecs != timer->Elapsed())
		{
			timer->NavigateTo(currTimeSecs);
		}
	}
	ImGui::PopID();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(170);
	int viewTicks = static_cast<int>(renderView->viewTicks);
	ImGui::PushID("nav_noteSize");
	if (ImGui::SliderInt("", &viewTicks, 48, 7680, "", ImGuiSliderFlags_Logarithmic))
	{
		renderView->viewTicks = std::clamp(viewTicks, 48, 7680);
	}
	ImGui::PopID();

	ImGui::End();
	ImGui::PopStyleVar();

	Update();
}

void NavigationBar::Update()
{

}