#pragma once

#include <vector>
#include "DiagnosisField.h"
#include "../../App/MIDIApp.h"
#include "DiagnosisMessage.h"

class ADiagnosis
{
public:
	ADiagnosis(MIDIApp* app, const char* filePath)
		: app(app), file(filePath) { }
	virtual ~ADiagnosis() = default;

	virtual void Run() = 0;
	virtual void Stop() = 0;
	bool IsRunning() const
	{
		return isRunning;
	}
	virtual double GetProgress() = 0;
	std::vector<std::shared_ptr<DiagnosisField>>& GetFields()
	{
		return fields;
	}
	std::vector<std::shared_ptr<DiagnosisMessage>>& GetWarnings()
	{
		return warnings;
	}
protected:
	template <typename T>
	T* CreateField(const std::string& name)
	{
		static_assert(std::is_base_of_v<DiagnosisField, T>);

		auto field = std::make_unique<T>(name);
		T* fieldPtr = field.get();

		fields.push_back(std::move(field));

		return fieldPtr;
	}

	const char* file;
	MIDIApp* app;
	bool isRunning = false;

private:
	std::vector<std::shared_ptr<DiagnosisMessage>> warnings;
	std::vector<std::shared_ptr<DiagnosisField>> fields;
};