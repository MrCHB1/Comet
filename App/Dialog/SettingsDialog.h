#pragma once

#include "App/MIDIApp.h"
#include "App/UI/Dialog.h"

class SettingsDialog : public Dialog
{
public:
	SettingsDialog(MIDIApp* app) : Dialog("settingsDialog"), app(app) {}
	const char* GetTitle() override { return "Settings"; }
	void DrawContent() override;
	ImVec2 GetInitialSize() override { return ImVec2(760.0f, 650.0f); }
	ImGuiWindowFlags GetWindowFlags() override { return ImGuiWindowFlags_NoSavedSettings; }
private:
	MIDIApp* app;

	void DrawAppTab();
	void DrawVisualTab();
	void DrawAudioTab();
	void DrawMIDITab();
};