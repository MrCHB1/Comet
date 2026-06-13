#pragma once
#include <vector>
#include <string>

class FFmpegGPU
{
public:
	FFmpegGPU();
	const std::vector<std::string>& GetEncoderList() const;
	const std::string& GetCurrentEncoder() const { return currentEncoder; }
private:
	std::string currentEncoder = "";
	std::vector<std::string> availableEncoders;
};