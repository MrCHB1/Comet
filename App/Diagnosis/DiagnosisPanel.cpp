#include "DiagnosisPanel.h"
#include "imgui.h"
#include <chrono>
#include <iostream>

void DiagnosisPanel::SetDiagnoses(std::shared_ptr<Diagnoses> diagnoses)
{
	this->diagnoses = diagnoses;
	lastRunningIdx = 0;
	started = -1L;
	progs.clear();
	fields.clear();
	topLevelViews.clear();

	int i, l;

	for (i = 0, l = diagnoses->GetDiagnosisCount(); i < l; i++)
	{
		ADiagnosis* d = diagnoses->GetDiagnosis(i);
		progs[d] = 0;
	}

	for (i = 0, l = diagnoses->GetDiagnosisCount(); i < l; i++)
	{
		ADiagnosis* d = diagnoses->GetDiagnosis(i);
		if (i != 0)
		{

		}
		for (auto& field : d->GetFields())
		{
			std::shared_ptr<DiagnosisFieldView> view = DiagnosisFieldView::CreateView(app, d, field);
			fields[field] = view;
			topLevelViews.push_back(view);
			for (auto& afield : field->additionalFields)
			{
				std::shared_ptr<DiagnosisFieldView> aview = DiagnosisFieldView::CreateView(app, d, afield);
				view->additionalViews.push_back(aview);
				// aview->Init();
				fields[afield] = aview;
			}
		}
	}
}

void DiagnosisPanel::Draw()
{
	ImGui::BeginChild("##fileName", ImVec2(300, ImGui::GetTextLineHeightWithSpacing() * 2.5), 0, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetWindowFontScale(2.0f);
	ImGui::Text(titleLabel.c_str());
	ImGui::SetWindowFontScale(1.0f);
	ImGui::EndChild();
	ImGui::Spacing();

	ImGui::Text(statusLabel.c_str());
	ImGui::Separator();

	for (auto &field_view : topLevelViews)
	{
		field_view->Draw();
		for (auto& afield : field_view->additionalViews)
		{
			afield->Draw();
		}
	}
}

void DiagnosisPanel::Update()
{
	long currTimeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();;

	bool isRunning = diagnoses->IsRunning();
	int runningIdx = diagnoses->GetDiagnosisRunningIdx();
	std::vector<double> prog(diagnoses->GetDiagnosisCount());

	/*for (int i = lastRunningIdx, l = isRunning ? runningIdx : diagnoses->GetDiagnosisCount(); i < l; i++)
	{
		ADiagnosis* d = diagnoses->GetDiagnosis(i);
		// AddWarnings(d->GetWarnings());
	}*/

	for (int j = 0; j < diagnoses->GetDiagnosisCount(); j++)
	{
		ADiagnosis* d = diagnoses->GetDiagnosis(j);
		auto* p = &(progs[d]);
		prog[j] = d->GetProgress();
		progs[d] = (j == runningIdx) ? prog[j] : (j < runningIdx ? 1 : 0);
	}

	for (auto& [f, v] : fields)
	{
		v->Update();
	}

	if (isRunning)
	{
		if (runningIdx == lastRunningIdx)
		{
			if (started == -1L && prog[runningIdx] >= 0)
				started = currTimeMillis;
		}
		else
		{
			started = (prog[runningIdx] < 0) ? -1L : currTimeMillis;
		}

		if (started == -1L)
		{
			statusLabel = "Preparing diagnosis";
		}
		else
		{
			long elapsed = currTimeMillis - started;
			long total = (long)(elapsed / prog[runningIdx]);
			long remaining = total - elapsed + 1000L;
			statusLabel = std::to_string(remaining / 1000L) + "s remaining";
		}
	}
	else
	{
		statusLabel = "Diagnosis Finished!";
	}
	lastRunningIdx = runningIdx;
}