#include "RenderVideoDialog.h"
#include "FFmpeg/FFmpegCommandBuilder.h"
#include "Utils.h"
#include <filesystem>
#include "MIDI/MIDISequence.h"
#include "App/UI/Widgets/TimeRange.h"
#include "App/UI/Layout/ForumLayout.h"
#include "App/MIDIApp.h"
#include "App/Localization/Localization.h"

namespace
{
    using namespace Comet::UI::Layout;
}

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <climits>
#include <mach-o/dyld.h>
#else
#include <climits>
#include <unistd.h>
#endif

#include "DialogMacros.h"

const char* FFMPEG_DOWNLOAD_URL = "https://ffmpeg.org/download.html";

#define CLAMP_VALUE(n,a,b) \
	if (n < a) n = a; \
	if (n > b) n = b;

void RenderVideoDialog::OnOpen()
{
	DetectFFmpeg();
}

void RenderVideoDialog::OnRegister()
{
	// gpuEncoder = std::make_unique<FFmpegGPU>();
}

void RenderVideoDialog::DetectFFmpeg()
{
#if defined(_WIN32)
	const char* ffmpegName = "ffmpeg.exe";
	const char pathSeparator = ';';
#else
	const char* ffmpegName = "ffmpeg";
	const char pathSeparator = ':';
#endif

	auto isFFmpeg = [](const std::filesystem::path& p)
	{
		std::error_code ec;
		return std::filesystem::is_regular_file(p, ec);
	};

	// next to the Comet binary
	std::filesystem::path binDir = Utils::GetExecutableDirectory();
	if (!binDir.empty() && isFFmpeg(binDir / ffmpegName))
	{
		hasFFmpeg = true;
		return;
	}

	// anywhere on PATH, which is where the render command looks for it too
	hasFFmpeg = false;
	if (const char* pathEnv = std::getenv("PATH"))
	{
		std::stringstream ss(pathEnv);
		std::string dir;
		while (std::getline(ss, dir, pathSeparator))
		{
			if (!dir.empty() && isFFmpeg(std::filesystem::path(dir) / ffmpegName))
			{
				hasFFmpeg = true;
				return;
			}
		}
	}
}

void RenderVideoDialog::DrawContent()
{
    bool hasSequence = app->hasSequence;
    bool canRender = hasSequence && hasFFmpeg;

    Localization* localization = app->GetLocalization();

    {
        ForumLayout layout("##renderContent");
        layout.AddLargeHeader(TRANSLATE("dialog.render.input").c_str());

        layout.AddField("##ffmpegExec",
            TRANSLATE("dialog.render.input.ffmpeg").c_str(),
            [&]()
            {
                if (hasFFmpeg)
                    ImGui::Text(TRANSLATE("dialog.render.input.ffmpeg.installed").c_str());
                else
                {
                    ImGui::TextColored(
                        ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                        TRANSLATE("dialog.render.error.noffmpeg").c_str()
                    );

                    if (ImGui::SmallButton(TRANSLATE("dialog.render.input.ffmpeg.get").c_str()))
                    {
                        Utils::OpenURL(FFMPEG_DOWNLOAD_URL);
                    }

                    ImGui::SameLine();

                    if (ImGui::SmallButton(TRANSLATE("dialog.render.input.ffmpeg.detect").c_str()))
                    {
                        DetectFFmpeg();
                    }
                }
            });

        layout.AddField("##inputMidi",
            TRANSLATE("dialog.render.input.midi").c_str(),
            [&]()
            {
                if (hasSequence)
                {
                    ImGui::Text(TRANSLATE("dialog.render.input.midi.loaded").c_str());
                }
                else
                {
                    ImGui::TextColored(
                        ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                        TRANSLATE("dialog.render.error.nomidi").c_str()
                    );
                }
            });

        layout.AddField("##includeAudio",
            TRANSLATE("dialog.render.input.audio").c_str(),
            [&]()
            {
                ImGui::Checkbox("", &renderSettings.includeAudio);

                if (renderSettings.includeAudio)
                {
                    Utils::AddFilePickerField(
                        "",
                        renderSettings.audioPath,
                        "mp3,ogg,wav,flac"
                    );
                }
            });

        layout.AddLargeHeader(TRANSLATE("dialog.render.output").c_str());

        layout.AddField("##format",
            TRANSLATE("dialog.render.output.format").c_str(),
            [&]()
            {
                IMGUI_RADIO_BUTTON("MP4", renderSettings.outputFormat, RenderOutputFormat::MP4); ImGui::SameLine();
                IMGUI_RADIO_BUTTON("MOV", renderSettings.outputFormat, RenderOutputFormat::MOV); ImGui::SameLine();
                IMGUI_RADIO_BUTTON("AVI", renderSettings.outputFormat, RenderOutputFormat::AVI);
            });

        layout.AddFieldWithTooltip("##gpuEncoding",
            TRANSLATE("dialog.render.output.gpu").c_str(),
            TRANSLATE("dialog.render.output.gpu.tooltip").c_str(),
            [&]()
            {
                ImGui::Checkbox("", &renderSettings.useGPUEncoding);
            });

        layout.AddField("##codec",
            TRANSLATE("dialog.render.output.codec").c_str(),
            [&]()
            {
                if (renderSettings.useGPUEncoding)
                {
                    if (gpuEncoder == nullptr)
                    {
                        gpuEncoder = std::make_unique<FFmpegGPU>();
                        renderSettings.gpuEncoder = gpuEncoder->GetCurrentEncoder();
                    }

                    bool gpuEncoderAvailable = false;

                    if (gpuEncoder != nullptr)
                    {
                        const auto& encoderList = gpuEncoder->GetEncoderList();

                        if (!encoderList.empty())
                        {
                            gpuEncoderAvailable = true;

                            for (const auto& encoder : encoderList)
                            {
                                IMGUI_RADIO_BUTTON(encoder.c_str(), renderSettings.gpuEncoder, encoder);
                                ImGui::SameLine();
                            }

                            ImGui::NewLine();
                        }
                    }

                    if (!gpuEncoderAvailable)
                    {
                        ImGui::TextColored(
                            ImVec4(0.5f, 0.5f, 0.0f, 1.0f),
                            TRANSLATE("dialog.render.output.gpu.unavailable").c_str()
                        );
                    }
                }
                else
                {
                    IMGUI_RADIO_BUTTON("H.264", renderSettings.codec, RenderCodec::H264); ImGui::SameLine();
                    IMGUI_RADIO_BUTTON("H.265", renderSettings.codec, RenderCodec::H265);
                }
            });

        layout.AddFieldWithTooltip("##advancedEncoding",
            TRANSLATE("dialog.render.output.advanced").c_str(),
            TRANSLATE("dialog.render.output.advanced.tooltip").c_str(),
            [&]()
            {
                ImGui::Checkbox("##advancedEncoding", &renderSettings.allowAdvancedEncoding);

                if (renderSettings.allowAdvancedEncoding)
                {
                    static char buf[4096];

                    strncpy(buf, renderSettings.advancedEncodingOptions.c_str(), sizeof(buf));

                    buf[sizeof(buf) - 1] = '\0';

                    if (ImGui::InputTextMultiline("##advancedOptions", buf, sizeof(buf), ImVec2(-FLT_MIN, 100)))
                    {
                        renderSettings.advancedEncodingOptions = buf;
                    }
                }
            });

        layout.AddLargeHeader(TRANSLATE("dialog.render.encoding").c_str());

        layout.AddField("##presetCombo",
            TRANSLATE("dialog.render.encoding.preset").c_str(),
            [&]()
            {
                if (ImGui::BeginCombo("", FFmpegCommandBuilder::GetPreset(renderSettings.encodingPreset)))
                {
                    for (int i = 0; i <= RenderEncodingPreset::PLACEBO; i++)
                    {
                        const char* presetName = "";

                        switch (i)
                        {
                        case RenderEncodingPreset::ULTRAFAST:
                            presetName = "ultrafast";
                            break;

                        case RenderEncodingPreset::SUPERFAST:
                            presetName = "superfast";
                            break;

                        case RenderEncodingPreset::VERYFAST:
                            presetName = "veryfast";
                            break;

                        case RenderEncodingPreset::FASTER:
                            presetName = "faster";
                            break;

                        case RenderEncodingPreset::FAST:
                            presetName = "fast";
                            break;

                        case RenderEncodingPreset::MEDIUM:
                            presetName = "medium";
                            break;

                        case RenderEncodingPreset::SLOW:
                            presetName = "slow";
                            break;

                        case RenderEncodingPreset::SLOWER:
                            presetName = "slower";
                            break;

                        case RenderEncodingPreset::VERYSLOW:
                            presetName = "veryslow";
                            break;

                        case RenderEncodingPreset::PLACEBO:
                            presetName = "placebo";
                            break;
                        }

                        bool selected = renderSettings.encodingPreset == i;

                        if (ImGui::Selectable(presetName, selected))
                        {
                            renderSettings.encodingPreset = static_cast<RenderEncodingPreset>(i);
                        }

                        if (selected) ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            });

        layout.AddField("##bitrateMode",
            TRANSLATE("dialog.render.encoding.bitratemode").c_str(),
            [&]()
            {
                IMGUI_RADIO_BUTTON(TRANSLATE("dialog.render.encoding.brm.const").c_str(),
                    renderSettings.encodingBitrate, RenderEncodingBitrate::CONSTANT);
                ImGui::SameLine();
                IMGUI_RADIO_BUTTON(TRANSLATE("dialog.render.encoding.brm.var").c_str(),
                    renderSettings.encodingBitrate, RenderEncodingBitrate::VARIABLE);
            });

        if (renderSettings.encodingBitrate == RenderEncodingBitrate::CONSTANT)
        {
            layout.AddField("##bitrate",
                TRANSLATE("dialog.render.encoding.bitrate").c_str(),
                [&]()
                {
                    ImGui::InputInt("", &renderSettings.bitrateKbps);

                    if (renderSettings.bitrateKbps < 0)
                        renderSettings.bitrateKbps = 0;
                });
        }
        else
        {
            layout.AddField("##crf",
                TRANSLATE("dialog.render.encoding.crf").c_str(),
                [&]()
                {
                    ImGui::InputInt("", &renderSettings.crf);
                    CLAMP_VALUE(renderSettings.crf, 0, 51);
                });
        }

        layout.AddLargeHeader(TRANSLATE("dialog.render.rendering").c_str());

        layout.AddField("##resolution",
            TRANSLATE("dialog.render.rendering.resolution").c_str(),
            [&]()
            {
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("##width", &renderSettings.width, 0, 0);
                CLAMP_VALUE(renderSettings.width, 128, 16384);

                ImGui::SameLine();
                ImGui::Text("x");
                ImGui::SameLine();

                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("##height", &renderSettings.height, 0, 0);
                CLAMP_VALUE(renderSettings.height, 128, 16384);
            });

        layout.AddField("##framerate",
            TRANSLATE("dialog.render.rendering.framerate").c_str(),
            [&]()
            {
                ImGui::SetNextItemWidth(100);

                ImGui::InputInt("", &renderSettings.fps, 0, 0);

                if (renderSettings.fps < 1)
                    renderSettings.fps = 1;
            });

        const char* extension = "mp4";

        switch (renderSettings.outputFormat)
        {
        case RenderOutputFormat::AVI:
            extension = "avi";
            break;

        case RenderOutputFormat::MP4:
            extension = "mp4";
            break;

        case RenderOutputFormat::MOV:
            extension = "mov";
            break;
        }

        layout.AddField("##outPath",
            TRANSLATE("dialog.render.rendering.outpath").c_str(),
            [&]()
            {
                Utils::AddFilePickerField("##outPath", renderSettings.outputPath, extension, true);

                if (renderSettings.outputPath.empty())
                {
                    ImGui::TextColored(
                        ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                        TRANSLATE("dialog.render.error.nooutpath").c_str()
                    );

                    canRender = false;
                }
            });

        layout.AddField("##alphaOutPath",
            TRANSLATE("dialog.render.rendering.mask").c_str(),
            [&]()
            {
                ImGui::Checkbox(
                    "##transparencyMask",
                    &renderSettings.renderTransparencyMask
                );

                if (renderSettings.renderTransparencyMask)
                {
                    Utils::AddFilePickerField("##alphaOutPath", renderSettings.maskOutputPath, extension, true);

                    if (renderSettings.maskOutputPath.empty())
                    {
                        ImGui::TextColored(
                            ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                            TRANSLATE("dialog.render.error.nooutmaskpath").c_str()
                        );
                        canRender = false;
                    }
                }
            });

        layout.AddFieldWithTooltip("##renderRange",
            TRANSLATE("dialog.render.rendering.renderrange").c_str(),
            TRANSLATE("dialog.render.rendering.renderrange.tooltip").c_str(),
            [&]()
            {
                ImGui::Checkbox("", &renderSettings.renderRange);
                if (renderSettings.renderRange)
                {
                    MIDISequence* seq = app->GetRendererSequence();
                    double seqDuration = seq ? (double)seq->GetLength() / 1000.0 : 60.0;

                    TimeRange::Draw("##renderRange",
                        &renderSettings.rangeStart,
                        &renderSettings.rangeEnd,
                        -renderSettings.midiStartDelay,
                        seqDuration + 5.0);

                    if (renderSettings.rangeStart > renderSettings.rangeEnd)
                    {
                        ImGui::TextColored(
                            ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                            TRANSLATE("dialog.render.error.invalidrange").c_str()
                        );
                        canRender = false;
                    }
                }
            });
    }
   
    ImGui::Spacing();

    ImGui::BeginDisabled(!canRender);

    if (ImGui::Button(TRANSLATE("dialog.render.renderbtn").c_str(), ImVec2(-FLT_MIN, 0)))
    {
        app->RenderMIDIVideo(renderSettings);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(TRANSLATE("dialog.render.renderbtn.tooltip").c_str());
    }

    ImGui::EndDisabled();

    if (!canRender)
    {
        ImGui::TextColored(
            ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
            TRANSLATE("dialog.render.error").c_str()
        );
    }

    if (ImGui::Button(TRANSLATE("dialog.close").c_str()))
    {
        ImGui::CloseCurrentPopup();
    }
}