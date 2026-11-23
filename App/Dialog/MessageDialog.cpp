#include "MessageDialog.h"
#include "App/Localization/Localization.h"

void MessageDialog::DrawContent()
{
	// wrap long messages nicely
	ImGui::TextWrapped("%s", message.c_str());
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (buttonType == ButtonType::OK)
	{
		if (ImGui::Button(TRANSLATE("dialog.ok").c_str(), ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::OK);
			Close();
		}
	}
	else if (buttonType == ButtonType::YesNo)
	{
		if (ImGui::Button(TRANSLATE("dialog.yes").c_str(), ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::Yes);
			Close();
		}
		ImGui::SameLine();
		if (ImGui::Button(TRANSLATE("dialog.no").c_str(), ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::No);
			Close();
		}
	}
}