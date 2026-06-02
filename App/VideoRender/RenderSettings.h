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
	std::string audioPath = "";
	std::string outputPath = "";

	RenderOutputFormat outputFormat = RenderOutputFormat::MP4;
	RenderCodec codec = RenderCodec::H264;
	RenderEncodingPreset encodingPreset = RenderEncodingPreset::VERYFAST;
	RenderEncodingBitrate encodingBitrate = RenderEncodingBitrate::VARIABLE;
	int crf = 23; // for variable bitrate encoding, lower means better quality but larger file size. typically ranges from 18 to 28.
	
	bool allowAdvancedEncoding = false;
	std::string advancedEncodingOptions = "";
};

static const char* GetExtension(RenderOutputFormat format)
{
	switch (format)
	{
		case MP4: return ".mp4";
		case MOV: return ".mov";
		case AVI: return ".avi";
		default:  return ".mp4";
	}
}

static std::string GetOutputPath(const RenderSettings& settings)
{
	std::filesystem::path path(settings.outputPath);
	path.replace_extension(GetExtension(settings.outputFormat));
	return path.string();
}

static const char* GetCodec(RenderCodec codec)
{
	switch (codec)
	{
		case H264: return "libx264";
		case H265: return "libx265";
		default:   return "libx264";
	}
}

static const char* GetPreset(RenderEncodingPreset preset)
{
	switch (preset)
	{
		case ULTRAFAST: return "ultrafast";
		case SUPERFAST: return "superfast";
		case VERYFAST:  return "veryfast";
		case FASTER:    return "faster";
		case FAST:      return "fast";
		case MEDIUM:    return "medium";
		case SLOW:      return "slow";
		case SLOWER:    return "slower";
		case VERYSLOW:  return "veryslow";
		case PLACEBO:   return "placebo";
		default:        return "veryfast";
	}
}

inline std::string BuildFFmpegCommand(const RenderSettings& settings)
{
	std::stringstream cmd;

	cmd << "ffmpeg ";

	// overwrite existing file
	cmd << "-y ";

	// input
	cmd << "-f rawvideo ";
	cmd << "-pix_fmt rgba ";
	cmd << "-s "
		<< settings.width
		<< "x"
		<< settings.height
		<< " ";

	cmd << "-r " << settings.fps << " ";
	cmd << "-i - ";

	// attach audio, if possible
	if (settings.includeAudio)
	{
		cmd << "-itsoffset " << settings.midiStartDelay << " "
			<< "-i \"" << settings.audioPath << "\" ";
	}

	// codec
	cmd << "-c:v " << GetCodec(settings.codec) << " ";

	// preset
	cmd << "-preset " << GetPreset(settings.encodingPreset) << " ";

	// bitrate mode
	if (settings.encodingBitrate == CONSTANT)
	{
		cmd << "-b:v "
			<< settings.bitrateKbps
			<< "k ";

		// encourage true-ish CBR
		cmd << "-minrate "
			<< settings.bitrateKbps
			<< "k ";

		cmd << "-maxrate "
			<< settings.bitrateKbps
			<< "k ";

		cmd << "-bufsize "
			<< (settings.bitrateKbps * 2)
			<< "k ";
	}
	else
	{
		cmd << "-crf "
			<< settings.crf
			<< " ";
	}

	// compatibility
	if (settings.codec == H264)
	{
		cmd << "-pix_fmt yuv420p ";
	}

	// user-specified extra arguments
	if (settings.allowAdvancedEncoding &&
		!settings.advancedEncodingOptions.empty())
	{
		cmd << settings.advancedEncodingOptions << " ";
	}

	// output file
	switch (settings.outputFormat)
	{
		case MP4:
			cmd << "-f mp4 ";
			break;

		case MOV:
			cmd << "-f mov ";
			break;

		case AVI:
			cmd << "-f avi ";
			break;
	}
	// vertically flip the video
	cmd << "-vf vflip ";
	cmd << "\"" << GetOutputPath(settings) << "\"";

	return cmd.str();
}