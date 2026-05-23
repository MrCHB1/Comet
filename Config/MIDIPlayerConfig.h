#pragma once

#include "ConfigSection.h"

struct MIDIPlayerConfig
{
	struct ConfigOverlayInfo
	{
		const float MIN_SCALE = 0.25f;
		const float MAX_SCALE = 8.0f;
	private:
		float scale = 1.0f;
		bool showDuration = false;
	};

	struct ConfigOverlay
	{
		bool opaque;

	};

	struct ConfigMIDI
	{
		int loaderThreads = 0;
		bool loadNotesOnly = false;
		bool usePlayThread = true;
		std::string synth;
	};

	struct ConfigRender
	{
		int GetWidth() { return width; }
		void SetWidth(int width)
		{
			this->width = width;
			if (this->width > 16384)
				this->width = 16384;
			else if (this->width < 128)
				this->width = 128;
		}
		int GetHeight() { return height; }
		void SetHeight(int height)
		{
			this->height = height;
			if (this->height > 16384)
				this->height = 16384;
			else if (this->height < 200)
				this->height = 200;
		}
		int GetFPSLimit()
		{
			return fpsLimit;
		}
		ImVec4 GetBackground()
		{
			return background;
		}
	private:
		int width = 1280;
		int height = 720;
		int fpsLimit = 120;
		double renderInterval = 5.0;
		ImVec4 background = ImVec4(0.75, 0.75, 0.75, 1.0);
		bool usePFAColors = false;
		std::string resourcePack = "";
		std::string font = "Monospaced";
		MIDIPlayerConfig::ConfigOverlay overlay{};
	};

	int version = 0;
	std::string language = "en";
	ConfigMIDI midi{};
	ConfigRender render{};
	// ConfigInterface ui{};
	// ConfigFiles files{};
	// ConfigUpdate updateChecker{};

	
};