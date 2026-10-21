#pragma once
#include "GPUImage.h"
#include <random>
#include <vector>
#include <array>
#include <memory>

struct ImVec4;

class ColorAsset
{
public:
	std::vector<ImVec4> colors{};

	ColorAsset() : mt(rd()), random(0.0f, 1.0f) {}
	void ResetColors();
	void LoadColors(uint16_t trackCount = 16);
	void LoadColors(const std::vector<std::array<float, 3>>& colors, bool loopColors = false, size_t requiredCount = 16 * 32);
	void LoadColors(std::shared_ptr<std::istream> paletteFile, bool loopColors = false);
	
	void ShuffleColors(uint32_t seed);
	// restores the colors from being shuffled
	void RestoreColors();
	
	size_t GetNumLoadedColors() { return numLoadedColors; }
	uint32_t GetColor(uint16_t track, uint8_t channel);

	[[nodiscard]] bool IsShuffled() const { return shuffleColors; }
protected:
	ImVec4 CreateRandomColor();
private:
	std::random_device rd;
	std::mt19937 mt;
	std::uniform_real_distribution<float> random;

	// stores the indices of each color.
	std::vector<size_t> colorIDs{};
	void UpdateColorIDs(size_t count);

	bool hasColors = false;
	bool loopColors = false;

	bool shuffleColors = false;
	uint32_t shuffleSeed = 0;

	size_t numLoadedColors = 0; // if loading via a color palette or a resource pack, this number will not be zero
};