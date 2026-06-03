#include "Diagnoses.h"
#include "DiagnosisField.h"
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <sstream>

void Diagnoses::RunAll()
{
	auto now = std::chrono::system_clock::now();
	long currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	isRunning = true;
	timeStarted = currentMillis;
	try
	{
		for (int i = 0; i < diagnoses.size() && isRunning; i++)
		{
			ADiagnosis* d = (diagnoses[i]).get();
			{
				std::lock_guard<std::mutex> lock(diagnosesMtx);
				diagnosisRunning = d;
				diagnosisIdx = i;
			}
			d->Run();
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Diagnosis failed" << std::endl;
	}

	isRunning = false;
}

std::string Diagnoses::CreateSummaryText()
{
	std::stringstream summary;
	for (int i = 0; i < diagnoses.size(); i++)
	{
		ADiagnosis* d = diagnoses[i].get();
		for (const auto& f : d->GetFields())
		{
			auto kv = dynamic_cast<KeyValue*>(f.get());
			if (!kv) continue;
			summary << kv->name << ": " << kv->GetValue() << "\n";
		}
	}
	return summary.str();
}