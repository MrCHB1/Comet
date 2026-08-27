#include "RenderVideoDialog.h"
#include "FFmpeg/FFmpegCommandBuilder.h"
#include "Utils.h"
#include <filesystem>
#include "MIDI/MIDISequence.h"
#include "App/UI/Widgets/TimeRange.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__APPLE__)
#include <climits>
#include <mach-o/dyld.h>
#else
#include <climits>
#include <unistd.h>
#endif

#include "DialogMacros.h"

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

    SECTION_HEADER_LARGE("Input");
    BEGIN_SECTION("##input")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("FFmpeg executable"),
            {
                if (hasFFmpeg)
                {
                    ImGui::Text("FFmpeg is installed!");
                }
                else
                {
                    ImGui::TextColored(
                        ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                        "No FFmpeg found!"
                    );

                    if (ImGui::SmallButton("Get FFmpeg"))
                    {
                        Utils::OpenURL(FFMPEG_DOWNLOAD_URL);
                    }

                    ImGui::SameLine();

                    if (ImGui::SmallButton("Detect FFmpeg"))
                    {
                        DetectFFmpeg();
                    }
                }
            });

        SECTION_ENTRY(SECTION_LABEL("MIDI"),
            {
                if (hasSequence)
                {
                    ImGui::Text("Will render the currently loaded MIDI");
                }
                else
                {
                    ImGui::TextColored(
                        ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
                        "No MIDI loaded"
                    );
                }
            });

        SECTION_ENTRY(SECTION_LABEL("Include audio"),
            {
                ImGui::Checkbox(
                    "##includeAudio",
                    &renderSettings.includeAudio
                );

                if (renderSettings.includeAudio)
                {
                    Utils::AddFilePickerField(
                        "",
                        renderSettings.audioPath,
                        "mp3,ogg,wav,flac"
                    );
                }
            });
    }
    END_SECTION;

    SECTION_HEADER_LARGE("Output");
    BEGIN_SECTION("##output")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("Format"),
            {
                IMGUI_RADIO_BUTTON("MP4", renderSettings.outputFormat, RenderOutputFormat::MP4); ImGui::SameLine();
                IMGUI_RADIO_BUTTON("MOV", renderSettings.outputFormat, RenderOutputFormat::MOV); ImGui::SameLine();
                IMGUI_RADIO_BUTTON("AVI", renderSettings.outputFormat, RenderOutputFormat::AVI);
            });

        SECTION_ENTRY(
            TABLE_LABEL_TOOLTIP(
                "GPU encoding",
                "Use hardware-accelerated FFmpeg encoders when available."
            ),
            {
                ImGui::Checkbox(
                    "##gpuEncoding",
                    &renderSettings.useGPUEncoding
                );
            });

        SECTION_ENTRY(SECTION_LABEL("Codec"),
            {
                if (renderSettings.useGPUEncoding)
                {
                    if (gpuEncoder == nullptr)
                    {
                        gpuEncoder = std::make_unique<FFmpegGPU>();
                        renderSettings.gpuEncoder =
                            gpuEncoder->GetCurrentEncoder();
                    }

                    bool gpuEncoderAvailable = false;

                    if (gpuEncoder != nullptr)
                    {
                        const auto& encoderList =
                            gpuEncoder->GetEncoderList();

                        if (!encoderList.empty())
                        {
                            gpuEncoderAvailable = true;

                            for (const auto& encoder : encoderList)
                            {
                                IMGUI_RADIO_BUTTON(
                                    encoder.c_str(),
                                    renderSettings.gpuEncoder,
                                    encoder
                                );

                                ImGui::SameLine();
                            }

                            ImGui::NewLine();
                        }
                    }

                    if (!gpuEncoderAvailable)
                    {
                        ImGui::TextColored(
                            ImVec4(0.5f, 0.5f, 0.0f, 1.0f),
                            "GPU encoding is not currently available. "
                            "Ensure FFmpeg is installed properly and "
                            "your drivers are updated."
                        );
                    }
                }
                else
                {
                    IMGUI_RADIO_BUTTON("H.264", renderSettings.codec, RenderCodec::H264); ImGui::SameLine();
                    IMGUI_RADIO_BUTTON("H.265", renderSettings.codec, RenderCodec::H265); 
                }
            });

        SECTION_ENTRY(
            TABLE_LABEL_TOOLTIP(
                "Advanced options",
                "Additional FFmpeg encoding options for advanced users. Only use this if you know what you're doing."
            ),
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

        END_SECTION;
    }

    SECTION_HEADER_LARGE("Encoding");
    BEGIN_SECTION("##encoding")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("Preset"),
            {
                if (ImGui::BeginCombo("##presetCombo", FFmpegCommandBuilder::GetPreset(renderSettings.encodingPreset)))
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

        SECTION_ENTRY(SECTION_LABEL("Bitrate mode"),
            {
                IMGUI_RADIO_BUTTON("Constant", renderSettings.encodingBitrate, RenderEncodingBitrate::CONSTANT); ImGui::SameLine();
                IMGUI_RADIO_BUTTON("Variable", renderSettings.encodingBitrate, RenderEncodingBitrate::VARIABLE);
            });

        if (renderSettings.encodingBitrate == RenderEncodingBitrate::CONSTANT)
        {
            SECTION_ENTRY(SECTION_LABEL("Bitrate (Kbps)"),
                {
                    ImGui::InputInt("##bitrate", &renderSettings.bitrateKbps);

                    if (renderSettings.bitrateKbps < 0)
                        renderSettings.bitrateKbps = 0;
                });
        }
        else
        {
            SECTION_ENTRY(SECTION_LABEL("CRF"),
                {
                    ImGui::InputInt("##crf", &renderSettings.crf);
                    CLAMP_VALUE(renderSettings.crf, 0, 51);
                });
        }
        END_SECTION;
    }
    
    SECTION_HEADER_LARGE("Rendering");

    BEGIN_SECTION("##rendering")
    {
        SETUP_SECTION;

        SECTION_ENTRY(SECTION_LABEL("Resolution"),
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

        SECTION_ENTRY(SECTION_LABEL("Framerate"),
            {
                ImGui::SetNextItemWidth(100);

                ImGui::InputInt("##framerate", &renderSettings.fps, 0, 0);

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

        SECTION_ENTRY(SECTION_LABEL("Output path"),
            {
                Utils::AddFilePickerField("##outPath", renderSettings.outputPath, extension, true);

                if (renderSettings.outputPath.empty())
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Please specify the video's output path.");

                    canRender = false;
                }
            });

        SECTION_ENTRY(SECTION_LABEL("Transparency mask"),
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
                        ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Please specify the transparency mask's output path.");
                        canRender = false;
                    }
                }
            });

        SECTION_ENTRY(TABLE_LABEL_TOOLTIP("Render range", "Renders the MIDI at a specified time interval. Useful for sharing short previews of an entire MIDI."),
            {
                ImGui::Checkbox("##renderRange", &renderSettings.renderRange);
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
                        ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Invalid render range.");
                        canRender = false;
                    }
                }
            });
        END_SECTION;
    }
   
    ImGui::Spacing();

    ImGui::BeginDisabled(!canRender);

    if (ImGui::Button("Render!", ImVec2(-FLT_MIN, 0)))
    {
        app->RenderMIDIVideo(renderSettings);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Press 'esc' to cancel the render at any time. "
            "Video will be saved."
        );
    }

    ImGui::EndDisabled();

    if (!canRender)
    {
        ImGui::TextColored(
            ImVec4(0.5f, 0.0f, 0.0f, 1.0f),
            "Cannot render; check above for errors."
        );
    }

    if (ImGui::Button("Close"))
    {
        ImGui::CloseCurrentPopup();
    }
}