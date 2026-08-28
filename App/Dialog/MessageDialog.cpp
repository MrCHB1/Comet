#include "MessageDialog.h"

void MessageDialog::DrawContent()
{
	// wrap long messages nicely
	ImGui::TextWrapped("%s", message.c_str());
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (buttonType == ButtonType::OK)
	{
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::OK);
			Close();
		}
	}
	else if (buttonType == ButtonType::YesNo)
	{
		if (ImGui::Button("Yes", ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::Yes);
			Close();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(120, 0)))
		{
			if (onResult) onResult(Result::No);
			Close();
		}
	}
}