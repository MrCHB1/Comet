#include "FFmpegGPU.h"
#include <iostream>
#include <string>
#include <array>
#include <memory>

FFmpegGPU::FFmpegGPU()
{
	std::cout << "Initializing GPU encoder list..." << std::endl;

#ifdef _WIN32
	std::string nullOut = " > NUL 2>&1";
#else
	std::string nullOut = " > /dev/null 2>&1";
#endif

	std::string hwEncoders[9]{
		"h264_nvenc",        // NVIDIA H.264
		"hevc_nvenc",        // NVIDIA HEVC
		"h264_amf",          // AMD H.264
		"hevc_amf",          // AMD HEVC
		"h264_qsv",          // Intel QuickSync H.264
		"hevc_qsv",          // Intel QuickSync HEVC
		"h264_videotoolbox", // Apple Mac H.264
		"hevc_videotoolbox", // Apple Mac HEVC
		"h264_vaapi"         // Linux Generic Hardware H.264
	};

	for (const auto& enc : hwEncoders)
	{
		std::string command = "ffmpeg -v error -f lavfi -i nullsrc -c:v " +
			enc + " -frames:v 1 -f null -" + nullOut;

		if (std::system(command.c_str()) != 0) continue;
		availableEncoders.push_back(enc);
	}

	std::cout << "Initialized successfully. Available GPU encoders: " << std::endl;
	if (availableEncoders.size() > 0)
	{
		for (const auto& enc : availableEncoders)
		{
			std::cout << "  " << enc << std::endl;
		}

		currentEncoder = availableEncoders[0];
	}
	else
	{
		std::cout << "  No GPU encoders :(" << std::endl;
	}
}

const std::vector<std::string>& FFmpegGPU::GetEncoderList() const
{
	return availableEncoders;
}