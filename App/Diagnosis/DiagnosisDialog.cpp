#include "DiagnosisDialog.h"
#include "../../MIDI/Diagnosis/BasicDiagnosis.h"
#include "../../Utils.h"
#include <thread>
#include <nfd.h>
#include <filesystem>

void DiagnosisDialog::DrawContent()
{
	if (isDiagnosing)
	{
		diagnosisInfoPanel->Draw();
		ImGui::Separator();
		if (diagnoses && diagnoses->IsRunning())
		{
			if (ImGui::Button("Stop"))
			{
				SetRunning(false);
				diagnoses->Stop();
				timer->Stop();
				diagnosisInfoPanel->SetStatus("Diagnosis stopped.");
			}
		}
		else
		{
			if (ImGui::Button("Diagnose another"))
			{
				isDiagnosing = false;
			}
		}
	}
	else
	{
		ImGui::Text("Also run:");
		ImGui::Checkbox("Black MIDI Diagnosis", &t_blackMidiDiagnosis);
		if (ImGui::Button("Drop MIDI file or click here to start", ImVec2(350, 500)))
		{
			std::string path;
			Utils::ChooseFile(path);
			if (!path.empty())
				Start(path.c_str());
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DIAGNOSE_MIDI"))
			{
				const char* dropped = (const char*)payload->Data;
				Start(dropped);
			}

			ImGui::EndDragDropTarget();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Close"))
	{
		if (isRunning)
		{
			if (diagnoses) diagnoses->Stop();
		}
		ImGui::CloseCurrentPopup();
	}
}

void DiagnosisDialog::Start(const char* path)
{
	if (worker.joinable()) worker.join();

	auto filePath = std::filesystem::path(path);

	{
		auto diagnosesPtr = this->diagnoses.get();
		if (diagnosesPtr)
		{
			diagnosesPtr->Stop();
		}
	}

	SetRunning(true);
	std::vector<std::unique_ptr<ADiagnosis>> diagnoses{};
	diagnoses.push_back(std::make_unique<BasicDiagnosis>(app, path));
	if (t_blackMidiDiagnosis)
	{
		diagnoses.push_back(std::make_unique<BlackMIDIDiagnosis>(app, path));
	}
	this->diagnoses = std::make_shared<Diagnoses>(std::move(diagnoses));
	this->diagnosisInfoPanel->SetTitle(filePath.filename().string());
	this->diagnosisInfoPanel->SetDiagnoses(this->diagnoses);
	
	timer->Start();
	auto diag = this->diagnoses;
	worker = std::thread([diag]() {
		diag->RunAll();
	});
}