#pragma once
#include "../UI/Dialog.h"
#include "../Progress.h"
#include "../MIDIApp.h"

class LoadingDialog : public Dialog
{
public:
	LoadingDialog(MIDIApp* app) : Dialog("loading"), app(app)
	{
		prog = app->GetProgress();
	}

	const char* GetTitle() override
	{
		return "Loading...";
	}
	void DrawContent() override;
private:
	std::shared_ptr<Progress> prog;
	MIDIApp* app;
};