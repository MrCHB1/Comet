#pragma once
#include "../UI/Dialog.h"
#include "../../MIDI/Diagnosis/Diagnoses.h"
#include "../../MIDI/Diagnosis/BlackMIDIDiagnosis.h"
#include "DiagnosisPanel.h"
#include "../MIDIApp.h"
#include <fstream>
#include "../../IntervalThread.h"

class DiagnosisDialog : public Dialog
{
public:
	DiagnosisDialog(MIDIApp* app) : Dialog("diagnosis"), app(app)
	{
		diagnosisInfoPanel = new DiagnosisPanel(app);
		timer = std::make_unique<IntervalThread>(16, [this]() {
			if (!isRunning) timer->Stop();
			diagnosisInfoPanel->Update();
		});
	}

	~DiagnosisDialog()
	{
		Stop();
	}

	const char* GetTitle() override
	{
		return "Diagnose MIDI";
	}

	void DrawContent() override;
	void Start(const char* path);
private:
	void SetRunning(bool running)
	{
		isRunning = running;
		if (running) isDiagnosing = true;
	}
	void Stop()
	{
		bool expected = true;
		if (!isRunning) return;
		if (timer) timer->Stop();
		if (worker.joinable()) worker.join();
	}

	bool t_blackMidiDiagnosis = false;
	bool isRunning = false;
	bool isDiagnosing = false;
	std::shared_ptr<Diagnoses> diagnoses;
	MIDIApp* app;
	DiagnosisPanel* diagnosisInfoPanel = nullptr;
	std::ifstream currDiagnosisFile;

	std::unique_ptr<IntervalThread> timer;
	std::thread worker;
};