#pragma once

#include "App/MIDIApp.h"
#include "App/UI/Dialog.h"

class SettingsDialog : public Dialog
{
public:
	SettingsDialog(MIDIApp* app) : Dialog("settingsDialog"), app(app) {}
	const char* GetTitle() override { return "Settings"; }
	void DrawContent() override;
private:
	MIDIApp* app;
};