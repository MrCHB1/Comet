#pragma once
#include "../MIDIApp.h"
#include "../../MIDI/Diagnosis/Diagnoses.h"
#include "DiagnosisFieldView.h"
#include <unordered_map>

class DiagnosisPanel
{
public:
	DiagnosisPanel(MIDIApp* app) : app(app) {};
	void Draw();
	void Update();
	void SetTitle(std::string title)
	{
		titleLabel = title;
	}
	void SetStatus(std::string status)
	{
		statusLabel = status;
	}
	void SetDiagnoses(std::shared_ptr<Diagnoses> diagnoses);
private:
	MIDIApp* app;
	std::unordered_map<ADiagnosis*, double> progs;
	std::unordered_map<std::shared_ptr<DiagnosisField>, std::shared_ptr<DiagnosisFieldView>> fields;
	std::vector<std::shared_ptr<DiagnosisFieldView>> topLevelViews;
	std::shared_ptr<Diagnoses> diagnoses;
	int lastRunningIdx = 0;
	long started = 0;

	std::string titleLabel;
	std::string statusLabel;
};