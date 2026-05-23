#include "DiagnosisFieldView.h"

std::shared_ptr<DiagnosisFieldView> DiagnosisFieldView::CreateView(MIDIApp* app, ADiagnosis* diagnosis, std::shared_ptr<DiagnosisField> field)
{
	auto kv = std::dynamic_pointer_cast<KeyValue>(field);
	if (kv) return std::make_shared<KeyValueView>(app, diagnosis, kv);
	auto ll = std::dynamic_pointer_cast<LongList>(field);
	if (ll) return std::make_shared<LongListView>(app, diagnosis, ll);
	throw std::exception("Unknown field type");
}
