#include "Diagnoses.h"
#include <chrono>
#include <stdexcept>
#include <iostream>

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