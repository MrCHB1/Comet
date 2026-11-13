#include "SoundfontList.h"

void SoundfontList::LoadFromPaths(const std::vector<std::wstring>& paths)
{
	ClearSoundfonts();

	for (const std::wstring& path : paths)
	{
		if (path.empty()) continue;
		AddSoundfont(path);
	}
}

void SoundfontList::LoadFromEntries(std::vector<SoundfontEntry> entries)
{
	ClearSoundfonts();

	for (SoundfontEntry& entry : entries)
	{
		if (entry.path.empty()) continue;
		AddSoundfont(std::move(entry));
	}
}

void SoundfontList::AddSoundfont(std::wstring path)
{
	AddSoundfont(SoundfontEntry{ .path = std::move(path) });
}

void SoundfontList::AddSoundfont(SoundfontEntry&& entry)
{
	soundfonts.push_back(std::move(entry));
}

void SoundfontList::RemoveSoundfont(std::size_t index)
{
	if (index >= soundfonts.size()) return;
	soundfonts.erase(soundfonts.begin() + static_cast<std::ptrdiff_t>(index));
}

bool SoundfontList::SwapSoundfonts(std::size_t a, std::size_t b)
{
	if (a >= soundfonts.size() || b >= soundfonts.size() || a == b) return false;
	std::swap(soundfonts[a], soundfonts[b]);
	return true;
}

SoundfontEntry* SoundfontList::GetSoundfont(std::size_t index)
{
	if (index >= soundfonts.size()) return nullptr;
	return &soundfonts[index];
}

const SoundfontEntry* SoundfontList::GetSoundfont(std::size_t index) const
{
	if (index >= soundfonts.size()) return nullptr;
	return &soundfonts[index];
}

std::vector<std::wstring> SoundfontList::GetActiveSoundfontPaths() const
{
	std::vector<std::wstring> active;
	for (const SoundfontEntry& sf : soundfonts)
	{
		if (sf.enabled) active.push_back(sf.path);
	}
	return active;
}