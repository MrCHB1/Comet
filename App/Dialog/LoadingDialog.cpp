#include "LoadingDialog.h"
#include "../../MIDI/AbstractMIDILoader.h"

void LoadingDialog::DrawContent() 
{
	bool pendingClose = false;
	prog = app->GetProgress();
	if (!prog)
	{
		ImGui::Text("Waiting for load...");
	}
	else
	{
		ImGui::Text(prog->GetName().c_str());

		for (int i = 0; i < prog->GetBarCount(); i++)
		{
			double pVal = prog->GetBar(i)();
			if (pVal > 0)
				ImGui::ProgressBar(pVal, ImVec2(0.0f, 0.0f));
		}

		if (ImGui::Button("Cancel"))
		{
			prog->Stop();
			pendingClose = true;
			std::cout << "Loading canceled" << std::endl;
		}
	}

	if (!app->IsLoading())
	{
		pendingClose = true;
	}

	if (pendingClose)
	{
		ImGui::CloseCurrentPopup();
	}
}