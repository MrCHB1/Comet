#include "FFmpegCommandBuilder.h"
#include <sstream>


std::string FFmpegCommandBuilder::BuildFFmpegCommand(const RenderSettings& settings)
{
    
    std::stringstream cmd;
    cmd << "ffmpeg ";
    cmd << "-y "; // overwrite existing file
    
    // input settings
    cmd << "-f rawvideo ";
    cmd << "-pix_fmt rgba ";
    cmd << "-s " << settings.width << "x" << settings.height << " ";
    cmd << "-r " << settings.fps << " ";
    cmd << "-i - "; // read from stdin
    
    // attach audio, if possible
    if (settings.includeAudio) {
        
#ifdef _WIN32
        auto u8path = settings.audioPath.u8string();
        std::string audioPath(u8path.begin(), u8path.end());
#else
        std::string audioPath = settings.audioPath.string();
#endif
        cmd << "-itsoffset " << settings.midiStartDelay << " ";
        cmd	<< "-i \"" << audioPath << "\" ";
    }
    
    // determine encoder profiles
    bool isGPU = settings.useGPUEncoding;
    std::string encoder = isGPU ? settings.gpuEncoder : GetCodec(settings.codec);
    
    bool isNVENC = isGPU && (encoder.find("nvenc") != std::string::npos);
    bool isQSV = isGPU && (encoder.find("qsv") != std::string::npos);
    bool isAMF = isGPU && (encoder.find("amf") != std::string::npos);
    bool isMac = isGPU && (encoder.find("videotoolbox") != std::string::npos);
    
    // apply codec
    std::stringstream encoderArgs;
    encoderArgs << "-c:v " << encoder << " ";
    
    if (!isGPU)
    {
        encoderArgs << "-preset " << GetPreset(settings.encodingPreset) << " ";
    }
    else if (isNVENC)
    {
        switch (settings.encodingPreset) {
            case ULTRAFAST: case SUPERFAST: encoderArgs << "-preset p1 "; break;
            case VERYFAST:  case FASTER:    encoderArgs << "-preset p2 "; break;
            case FAST:                      encoderArgs << "-preset p3 "; break;
            case MEDIUM:                    encoderArgs << "-preset p4 "; break; // default
            case SLOW:                      encoderArgs << "-preset p5 "; break;
            case SLOWER:                    encoderArgs << "-preset p6 "; break;
            case VERYSLOW:  case PLACEBO:   encoderArgs << "-preset p7 "; break;
        }
    }
    else if (isQSV)
    {
        encoderArgs << "-preset " << GetPreset(settings.encodingPreset) << " ";
    }
    // note: AMD (AMF) and Apple (videotoolbox) omit -preset entirely to avoid crashes
    
    if (settings.encodingBitrate == CONSTANT)
    {
        encoderArgs << "-b:v " << settings.bitrateKbps << "k ";
        
        if (!isGPU || isQSV)
        {
            encoderArgs << "-minrate " << settings.bitrateKbps << "k "
            << "-maxrate " << settings.bitrateKbps << "k "
            << "-bufsize " << (settings.bitrateKbps * 2) << "k ";
        }
    }
    else
    {
        if (!isGPU)
            encoderArgs << "-crf " << settings.crf << " ";
        else if (isNVENC)
            encoderArgs << "-rc vbr -cq " << settings.crf << " ";
        else if (isQSV)
            encoderArgs << "-global_quality " << settings.crf << " ";
        else if (isMac)
            encoderArgs << "-q:v " << settings.crf << " ";
        else {
            // fallback for AMF/VAAPI which struggle with direct CRF mapping
            encoderArgs << "-b:v " << settings.bitrateKbps << "k ";
        }
    }
    
    // if the string contains "h264" or the fallback codec is H264, force yuv420p
    if (encoder.find("h264") != std::string::npos || (!isGPU && settings.codec == H264))
    {
        encoderArgs << "-pix_fmt yuv420p ";
    }
    
    // user-specified extra arguments
    if (settings.allowAdvancedEncoding && !settings.advancedEncodingOptions.empty())
    {
        encoderArgs << settings.advancedEncodingOptions << " ";
    }
    
    // output file format packaging
    std::string formatFlag;
    switch (settings.outputFormat)
    {
        case MP4: formatFlag = "-f mp4 "; break;
        case MOV: formatFlag = "-f mov "; break;
        case AVI: formatFlag = "-f avi "; break;
    }
    
    if (settings.renderTransparencyMask)
    {
        cmd << "-filter_complex \"[0:v]vflip,split=2[main_out][alpha_pre];[alpha_pre]alphaextract[mask_out]\" ";
        
        cmd << "-map \"[main_out]\" ";
        if (settings.includeAudio)
            cmd << "-map 1:a ";
        cmd << encoderArgs.str() << formatFlag;
        cmd << "\"" << GetOutputPath(settings) << "\" ";
        
        cmd << "-map \"[mask_out]\" " << encoderArgs.str() << formatFlag;
        
        if (encoderArgs.str().find("yuv420p") == std::string::npos)
            cmd << "-pix_fmt yuv420p ";
        
        cmd << "\"" << GetMaskOutputPath(settings) << "\"";
    }
    else
    {
        cmd << "-vf vflip ";
        cmd << encoderArgs.str() << formatFlag;
        cmd << "-shortest \"" << GetOutputPath(settings) << "\"";
        
        
        return cmd.str();
    }
}
    const char* FFmpegCommandBuilder::GetExtension(RenderOutputFormat format)
{
        switch (format) {
                
            case MP4: return ".mp4";
            case MOV: return ".mov";
            case AVI: return ".avi";
            default:  return ".mp4";
                
        }
    }
    std::string FFmpegCommandBuilder::GetOutputPath(const RenderSettings& settings) {
        
        std::filesystem::path path(settings.outputPath);
        path.replace_extension(GetExtension(settings.outputFormat));
        
#ifdef _WIN32
        auto u8str = path.u8string();
        return std::string(u8str.begin(), u8str.end());
#else
        return path.string();
#endif
    }
    
    std::string FFmpegCommandBuilder::GetMaskOutputPath(const RenderSettings& settings)
    {
    
        std::filesystem::path path(settings.maskOutputPath);
        path.replace_extension(GetExtension(settings.outputFormat));
        
#ifdef _WIN32
        auto u8str = path.u8string();
        return std::string(u8str.begin(), u8str.end());
#else
        return path.string();
#endif
    }
    
    const char* FFmpegCommandBuilder::GetCodec(RenderCodec codec)
{
        switch (codec) {
                
            case H264: return "libx264";
            case H265: return "libx265";
            default:   return "libx264";
        }
    }
    
    const char* FFmpegCommandBuilder::GetPreset(RenderEncodingPreset preset)
{
switch (preset) {
            
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
