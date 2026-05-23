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
		ImGui::Text(prog->GetName());

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