#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct SoundfontEntry
{
	std::wstring path;
	bool enabled = true;

	bool operator==(const SoundfontEntry&) const = default;
};

class SoundfontList
{
public:
	SoundfontList() = default;
	void LoadFromPaths(const std::vector<std::wstring>& paths);
	void LoadFromEntries(std::vector<SoundfontEntry> entries);

	void MarkAwaitingLoad() { awaitingLoad = true; }
	[[nodiscard]] bool IsAwaitingLoad() { return std::exchange(awaitingLoad, false); }

	void AddSoundfont(std::wstring path);
	void AddSoundfont(SoundfontEntry&& soundfont);
	void RemoveSoundfont(std::size_t index);
	bool SwapSoundfonts(std::size_t a, std::size_t b);

	[[nodiscard]] SoundfontEntry* GetSoundfont(std::size_t index);
	[[nodiscard]] const SoundfontEntry* GetSoundfont(std::size_t index) const;
	[[nodiscard]] std::size_t GetSoundfontCount() const { return soundfonts.size(); }

	// enabled soundfonts only, in list order
	[[nodiscard]] std::vector<std::wstring> GetActiveSoundfontPaths() const;

	void ClearSoundfonts() { soundfonts.clear(); }

	template <typename F>
	void ForEachSoundfont(F&& func) const
	{
		for (const SoundfontEntry& soundfont : soundfonts)
		{
			func(soundfont);
		}
	}
private:
	std::vector<SoundfontEntry> soundfonts{};
	bool awaitingLoad = false;
};