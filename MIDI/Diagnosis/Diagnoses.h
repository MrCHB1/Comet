#pragma once
#include <vector>
#include "ADiagnosis.h"
#include <mutex>

class Diagnoses
{
public:
	Diagnoses(std::vector<std::unique_ptr<ADiagnosis>> diagnoses) : diagnoses(std::move(diagnoses)) { }
	void RunAll();
	void Stop()
	{
		std::lock_guard<std::mutex> lock(diagnosesMtx);

		if (diagnosisRunning)
		{
			diagnosisRunning->Stop();
		}

		isRunning = false;
	}
	bool IsRunning() const { return isRunning; }
	int GetDiagnosisCount() const { return diagnoses.size(); }
	ADiagnosis* GetDiagnosis(int idx) { return (diagnoses[idx]).get(); }
	int GetDiagnosisRunningIdx()
	{
		std::lock_guard<std::mutex> lock(diagnosesMtx);
		return diagnosisIdx;
	}
	ADiagnosis* GetDiagnosesRunning()
	{
		std::lock_guard<std::mutex> lock(diagnosesMtx);
		return diagnosisRunning;
	}
	long GetTimeStarted()
	{
		return timeStarted;
	}
	std::string CreateSummaryText();
private:
	std::vector<std::unique_ptr<ADiagnosis>> diagnoses;
	int diagnosisIdx = 0;
	ADiagnosis* diagnosisRunning;
	bool isRunning = false;
	long timeStarted = 0;
	bool isDiagnosisException = false;
	
	std::mutex diagnosesMtx;
};