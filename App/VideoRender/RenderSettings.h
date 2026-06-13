#pragma once
#include <string>
#include <filesystem>

enum RenderOutputFormat
{
	MP4,
	MOV,
	AVI,
};

enum RenderCodec
{
	H264,
	H265
};

enum RenderEncodingPreset
{
	ULTRAFAST,
	SUPERFAST,
	VERYFAST,
	FASTER,
	FAST,
	MEDIUM,
	SLOW,
	SLOWER,
	VERYSLOW,
	PLACEBO
};

enum RenderEncodingBitrate
{
	CONSTANT,
	VARIABLE
};

struct RenderSettings
{
	int width = 1920;
	int height = 1080;
	int fps = 60;
	int bitrateKbps = 20000;

	double midiStartDelay = 3.0;
	bool includeAudio = false;

	std::filesystem::path audioPath = "";
	std::filesystem::path outputPath = "";
	bool renderTransparencyMask = false;
	std::filesystem::path maskOutputPath = "";

	RenderOutputFormat outputFormat = RenderOutputFormat::MP4;
	RenderCodec codec = RenderCodec::H264;
	bool useGPUEncoding = false;
	std::string gpuEncoder = "";
	RenderEncodingPreset encodingPreset = RenderEncodingPreset::VERYFAST;
	RenderEncodingBitrate encodingBitrate = RenderEncodingBitrate::VARIABLE;
	int crf = 20; // for variable bitrate encoding, lower means better quality but larger file size. typically ranges from 18 to 28.
	
	bool allowAdvancedEncoding = false;
	std::string advancedEncodingOptions = "";
};