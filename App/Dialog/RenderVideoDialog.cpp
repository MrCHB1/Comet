#include "RenderVideoDialog.h"
#include "Utils.h"
#include <filesystem>

#if defined(_WIN32)
	#include <windows.h>
#elif defined(__APPLE__)
	#include <cstdlib>
	#include <mach-o/dyld.h>
	#include <limits.h>
#else
	#include <unistd.h>
	#include <cstdlib>
	#include <limits>
#endif

std::filesystem::path GetBinaryDirectory()
{
#if defined(_WIN32)
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
	char buffer[PATH_MAX];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0)
		return std::filesystem::path(buffer).parent_path();
#else
	char buffer[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
	if (count != -1) {
		return std::filesystem::path(std::string(buffer, count)).parent_path();
	}
#endif

	return std::filesystem::path();
}

void OpenURL(const std::string& url)
{
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
	std::string command = "open " + url;
	system(command.c_str());
#else
	std::string command = "xdg-open " + url;
	system(command.c_str());
#endif
}

const char* FFMPEG_DOWNLOAD_URL = "https://ffmpeg.org/download.html";

#define CLAMP_VALUE(n,a,b) \
	if (n < a) n = a; \
	if (n > b) n = b;

void RenderVideoDialog::OnOpen()
{
	DetectFFmpeg();
}

void RenderVideoDialog::DetectFFmpeg()
{
	std::filesystem::path binDir = GetBinaryDirectory();
	if (binDir.empty())
	{
		hasFFmpeg = false;
		return;
	}

#if defined(_WIN32)
	std::filesystem::path ffmpegPath = binDir / "ffmpeg.exe";
#else
	std::filesystem::path ffmpegPath = binDir / "ffmpeg";
#endif

	hasFFmpeg = std::filesystem::exists(ffmpegPath) && std::filesystem::is_regular_file(ffmpegPath);
}

void RenderVideoDialog::DrawContent()
{
	bool hasSequence = app->hasSequence;
	bool canRender = hasSequence && hasFFmpeg;

	ImGui::SetWindowSize(ImVec2(750, 0), ImGuiCond_Once);
	// TODO: preview window here
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Input");
	ImGui::SetWindowFontScale(1.0f);

	ImGui::Text("FFmpeg executable");
	ImGui::SameLine();
	ImGui::PushID("btn_ffmpegBrowse");
	ImGui::Button("Browse...");
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(300);
	
	if (hasFFmpeg)
	{
		ImGui::Text("FFmpeg is installed!");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "No ffmpeg found! Please download it to render videos.");
	}
	
	if (ImGui::SmallButton("Get FFMPEG"))
	{
		// open browser to ffmpeg download page
		OpenURL(FFMPEG_DOWNLOAD_URL);
	}

	if (!hasFFmpeg)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Detect FFMPEG"))
		{
			DetectFFmpeg();
		}
	}

	ImGui::Text("MIDI");
	ImGui::SameLine();
	if (hasSequence)
	{
		ImGui::Text("Will render the currently loaded MIDI");
	}
	else
	{
		ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "You need to load a MIDI first before rendering.");
	}

	ImGui::Text("Include Audio");
	ImGui::SameLine();
	ImGui::Checkbox("##includeAudio", &renderSettings.includeAudio);
	
	if (renderSettings.includeAudio)
	{
		ImGui::PushID("btn_audioBrowse");
		if (ImGui::Button("Browse..."))
		{
			std::string inPath;
			if (Utils::ChooseFile(inPath))
			{
				renderSettings.audioPath = inPath;
			}
		}
		ImGui::PopID();
		ImGui::SameLine();
		char* buf = renderSettings.audioPath.data();
		ImGui::InputText("##audioInpath", buf, renderSettings.outputPath.size() + 1, ImGuiInputTextFlags_ReadOnly);
	}

	ImGui::Separator();
	ImGui::SetWindowFontScale(1.5f);
	ImGui::Text("Render Item");
	ImGui::SetWindowFontScale(1.0f);

	if (ImGui::BeginChild("RenderItem", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::Text("OUTPUT");
		ImGui::NewLine();

		ImGui::Text("Type");

		ImGui::SameLine();
		if (ImGui::RadioButton("mp4", renderSettings.outputFormat == RenderOutputFormat::MP4))
			renderSettings.outputFormat = RenderOutputFormat::MP4;

		ImGui::SameLine();
		if (ImGui::RadioButton("mov", renderSettings.outputFormat == RenderOutputFormat::MOV))
			renderSettings.outputFormat = RenderOutputFormat::MOV;

		ImGui::SameLine();
		if (ImGui::RadioButton("avi", renderSettings.outputFormat == RenderOutputFormat::AVI))
			renderSettings.outputFormat = RenderOutputFormat::AVI;

		ImGui::Text("Codec");

		ImGui::SameLine();
		if (ImGui::RadioButton("H.264", renderSettings.codec == RenderCodec::H264))
			renderSettings.codec = RenderCodec::H264;

		ImGui::SameLine();
		if (ImGui::RadioButton("H.265", renderSettings.codec == RenderCodec::H265))
			renderSettings.codec = RenderCodec::H265;

		ImGui::Checkbox("Type encoding options (for advanced FFMpeg users)", &renderSettings.allowAdvancedEncoding);
		if (renderSettings.allowAdvancedEncoding)
		{
			char* buf = renderSettings.advancedEncodingOptions.data();
			ImGui::InputTextMultiline("##advancedOptions", buf, renderSettings.advancedEncodingOptions.size() + 1);
		}

		ImGui::Separator();
		ImGui::Text("ENCODING");
		ImGui::NewLine();

		ImGui::Text("Preset");
		ImGui::SameLine();
		if (ImGui::BeginCombo("##presetCombo", nullptr))
		{
			for (int i = 0; i <= RenderEncodingPreset::PLACEBO; i++)
			{
				const char* presetName = "";
				switch (i)
				{
					case RenderEncodingPreset::ULTRAFAST: presetName = "ultrafast"; break;
					case RenderEncodingPreset::SUPERFAST: presetName = "superfast"; break;
					case RenderEncodingPreset::VERYFAST: presetName = "veryfast"; break;
					case RenderEncodingPreset::FASTER: presetName = "faster"; break;
					case RenderEncodingPreset::FAST: presetName = "fast"; break;
					case RenderEncodingPreset::MEDIUM: presetName = "medium"; break;
					case RenderEncodingPreset::SLOW: presetName = "slow"; break;
					case RenderEncodingPreset::SLOWER: presetName = "slower"; break;
					case RenderEncodingPreset::VERYSLOW: presetName = "veryslow"; break;
					case RenderEncodingPreset::PLACEBO: presetName = "placebo"; break;
				}
				bool selected = (renderSettings.encodingPreset == i);
				if (ImGui::Selectable(presetName, selected))
					renderSettings.encodingPreset = static_cast<RenderEncodingPreset>(i);
				
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Text("Target bitrate");
		if (ImGui::RadioButton("Constant", renderSettings.encodingBitrate == RenderEncodingBitrate::CONSTANT))
			renderSettings.encodingBitrate = RenderEncodingBitrate::CONSTANT;
		ImGui::SameLine();
		if (ImGui::RadioButton("Variable", renderSettings.encodingBitrate == RenderEncodingBitrate::VARIABLE))
			renderSettings.encodingBitrate = RenderEncodingBitrate::VARIABLE;

		if (renderSettings.encodingBitrate == RenderEncodingBitrate::CONSTANT)
		{
			ImGui::InputInt("Bitrate (Kbps)", &renderSettings.bitrateKbps);
			if (renderSettings.bitrateKbps < 0) renderSettings.bitrateKbps = 0;
		}
		else
		{
			ImGui::InputInt("CRF", &renderSettings.crf);
			CLAMP_VALUE(renderSettings.crf, 0, 51)
		}

		ImGui::Separator();
		ImGui::Text("RENDERING");
		ImGui::NewLine();

		ImGui::Text("Resolution");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("##width", &renderSettings.width, 0, 0);
		CLAMP_VALUE(renderSettings.width, 128, 16384)

		ImGui::SameLine();
		ImGui::Text("x");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("##height", &renderSettings.height, 0, 0);
		CLAMP_VALUE(renderSettings.height, 128, 16384);

		ImGui::Text("Framerate");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("##framerate", &renderSettings.fps, 0, 0);
		if (renderSettings.fps < 1) renderSettings.fps = 1;

		ImGui::Text("Output path");
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			std::string savePath = "";
			const char* extension;
			switch (renderSettings.outputFormat)
			{
				case RenderOutputFormat::MP4: extension = "mp4"; break;
				case RenderOutputFormat::MOV: extension = "mov"; break;
				case RenderOutputFormat::AVI: extension = "avi"; break;
			}
			if (Utils::SaveFile(savePath, extension))
			{
				renderSettings.outputPath = savePath;
			}
		}
		ImGui::SameLine();
		{
			char* buf = renderSettings.outputPath.data();
			ImGui::InputText("##videoOutpath", buf, renderSettings.outputPath.size() + 1, ImGuiInputTextFlags_ReadOnly);
		}
		if (renderSettings.outputPath.empty())
		{
			ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Please specify the video's output path.");
			canRender = false;
		}

		ImGui::EndChild();
	}

	ImGui::BeginDisabled(!canRender);
	if (ImGui::Button("Render!", ImVec2(-FLT_MIN, 0)))
	{
		app->RenderMIDIVideo(renderSettings);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Press 'esc' to cancel the render at any time. Video will be saved.");
	}
	ImGui::EndDisabled();
	if (!canRender)
	{
		ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Cannot render; check above for errors.");
	}

	if (ImGui::Button("Close"))
	{
		ImGui::CloseCurrentPopup();
	}
}