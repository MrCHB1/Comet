#pragma once
#include "App/VideoRender/RenderSettings.h"
#include "App/UI/Dialog.h"
#include "App/MIDIApp.h"

// the dialog for rendering a midi video. this is where the user can set the output path, video resolution, and other settings related to video rendering. it also shows a progress bar while the video is being rendered.
class RenderVideoDialog : public Dialog
{
public:
	RenderVideoDialog(MIDIApp* app) : Dialog("renderVideo"), app(app) {}
	const char* GetTitle() override
	{
		return "Render Video";
	}
	void DrawContent() override;
	void OnOpen() override;

	ImGuiWindowFlags GetWindowFlags() override
	{
		return ImGuiWindowFlags_None;
	}
	ImVec2 GetInitialSize() override
	{
		return ImVec2(750, 0);
	}

	RenderSettings renderSettings;
private:
	MIDIApp* app;
	bool hasFFmpeg = false;

	// called when dialog is opened but not rendering
	void DrawRenderSettingsContent();
	// called when dialog is open while video is rendering
	void DrawVideoRenderingContent();

	void DetectFFmpeg();
};