#pragma once

#include "App/MIDIApp.h"
#include "App/UI/Dialog.h"

class SettingsDialog : public Dialog
{
public:
	SettingsDialog(MIDIApp* app) : Dialog("settingsDialog"), app(app) {}
	const char* GetTitle() override { return "Settings"; }
	void DrawContent() override;
	ImVec2 GetInitialSize() override { return ImVec2(450, 0); }
	ImGuiWindowFlags GetWindowFlags() override { return ImGuiWindowFlags_HorizontalScrollbar; }
private:
	MIDIApp* app;

	void DrawAppTab();
	void DrawVisualTab();
	void DrawMIDITab();
};