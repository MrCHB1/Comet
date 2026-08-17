#pragma once

#include "App/VideoRender/RenderSettings.h"

class FFmpegCommandBuilder
{
public:
	static std::string BuildFFmpegCommand(const RenderSettings& settings);
	static const char* GetExtension(RenderOutputFormat format);
	static std::string GetOutputPath(const RenderSettings& settings);
	static std::string GetMaskOutputPath(const RenderSettings& settings);
	static const char* GetCodec(RenderCodec codec);
	static const char* GetPreset(RenderEncodingPreset preset);
};