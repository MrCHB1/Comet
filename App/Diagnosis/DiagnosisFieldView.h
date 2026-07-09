#pragma once

#include "../MIDIApp.h"
#include "../../MIDI/Diagnosis/Diagnoses.h"
#include <string>
#include "imgui.h"
#include <memory>

class DiagnosisFieldView
{
public:
	virtual ~DiagnosisFieldView() = default;

	virtual void Update() = 0;
	virtual void Draw() = 0;

	static std::shared_ptr<DiagnosisFieldView> CreateView(MIDIApp* app, ADiagnosis* diagnosis, std::shared_ptr<DiagnosisField> field);

	std::vector<std::shared_ptr<DiagnosisFieldView>> additionalViews{};
};

template <typename T>
class DiagnosisFieldViewT : public DiagnosisFieldView
{
protected:
	MIDIApp* app;
	ADiagnosis* diagnosis;
	std::shared_ptr<T> field;
public:
	DiagnosisFieldViewT(MIDIApp* app, ADiagnosis* diagnosis, std::shared_ptr<T> field)
		: app(app), diagnosis(diagnosis), field(field) {
		additionalViews = {};
	}
};

class KeyValueView : public DiagnosisFieldViewT<KeyValue>
{
public:
	using Base = DiagnosisFieldViewT;
	KeyValueView(MIDIApp* app, ADiagnosis* d, std::shared_ptr<KeyValue> f)
		: Base(app, d, f)
	{
		label = f->name;
		value = " ";
	}

	void Update() override
	{
		value = field->GetValue();
	}

	void Draw() override
	{
		ImGui::Text("%s", label.c_str());
		ImGui::SameLine();
		ImGui::Text("%s", value.c_str());
	}
private:
	std::string label;
	std::string value;
	bool isAdditionalViewsVisible = false;
};

class LongListView : public DiagnosisFieldViewT<LongList>
{
public:
	using Base = DiagnosisFieldViewT;
	LongListView(MIDIApp* app, ADiagnosis* d, std::shared_ptr<LongList> f)
		: Base(app, d, f)
	{

	}

	void Update() override {}
	void Draw() override
	{
		std::shared_ptr<std::vector<long>> arr = field->GetValue();
		if (!arr || arr->empty()) return;

		ImGui::PushID(field.get());

		ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x <= 0.0f) size.x = 100.0f;
		size.y = 80.0f;
		ImGui::InvisibleButton("##graphBounds", size);

		ImVec2 p0 = ImGui::GetItemRectMin();
		ImVec2 p1 = ImGui::GetItemRectMax();

		ImDrawList* draw = ImGui::GetWindowDrawList();

		// background
		draw->AddRectFilled(
			p0,
			p1,
			IM_COL32(30, 30, 30, 255)
		);

		long min = 0;
		long max = 1;

		for (const long& v : *arr)
		{
			if (v < min) min = v;
			if (v > max) max = v;
		}

		float d = float(max - min);
		if (d == 0.0f) d = 1.0f;

		float w = size.x - 2.0f;
		float h = size.y - 2.0f;

		float lastX = 0.0f;
		float lastY = 0.0f;

		bool first = true;

		for (size_t j = 0; j < arr->size(); j++)
		{
			float t = float(j) / float(arr->size() - 1);
			float v = float((*arr)[j]);

			float x = p0.x + 1.0f + t * w;
			float y = p0.y + 1.0f + (h - ((v - min) / d) * h);

			if (!first)
			{
				draw->AddLine(
					ImVec2(lastX, lastY),
					ImVec2(x, y),
					IM_COL32(80, 160, 255, 255),
					1.5f
				);
			}

			lastX = x;
			lastY = y;
			first = false;
		}

		ImGui::PopID();
	}
};
