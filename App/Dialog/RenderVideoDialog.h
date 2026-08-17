#pragma once
#include "App/VideoRender/RenderSettings.h"
#include "App/UI/Dialog.h"
#include "App/MIDIApp.h"
#include "FFmpeg/FFmpegGPU.h"

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
	void OnRegister() override;

	ImGuiWindowFlags GetWindowFlags() override
	{
		return ImGuiWindowFlags_None;
	}

	RenderSettings renderSettings;
private:
	MIDIApp* app;
	std::unique_ptr<FFmpegGPU> gpuEncoder;

	bool hasFFmpeg = false;
	void DetectFFmpeg();
	void AddFilePickerField(const char* label, std::filesystem::path& path, const char* extension = nullptr, bool saving = false);
};