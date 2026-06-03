#include "SettingsDialog.h"

void SettingsDialog::DrawContent()
{
	auto config = app->GetConfig();
	if (ImGui::BeginTabBar("MainTabs"))
	{
		if (ImGui::BeginTabItem("Visual"))
		{
			ImGui::Text("These settings will also apply when rendering videos!");
			ImGui::Separator();

			if (ImGui::BeginChild("##scrollArea", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar))
			{
				auto colors = config->render.GetBarColor();
				float barColor[3]{ colors.x, colors.y, colors.z };
				if (ImGui::ColorPicker3("Bar Color", barColor))
				{
					config->render.SetBarColor(barColor[0], barColor[1], barColor[2]);
					app->GetRenderer()->SetBarColor(barColor[0], barColor[1], barColor[2]);
				}

				colors = config->render.GetBackground();
					float bgColor[3]{ colors.x, colors.y, colors.z };
				if (ImGui::ColorPicker3("Background Color", bgColor))
				{
					config->render.SetBackground(bgColor[0], bgColor[1], bgColor[2]);
					app->GetRenderer()->SetBackgroundColor(bgColor[0], bgColor[1], bgColor[2]);
				}

				ImGui::BeginDisabled(true);
				ImGui::Button("Pick color palette");
				ImGui::Button("Pick new resource pack");
				ImGui::EndDisabled();

				ImGui::EndChild();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::Separator();
	if (ImGui::Button("Apply & Close"))
	{
		ImGui::CloseCurrentPopup();
	}
}