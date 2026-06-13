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
		bool multithreadedLoading = false;
		bool timeBasedLoading = false;
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
		void SetBackground(float r, float g, float b)
		{
			background.x = r;
			background.y = g;
			background.z = b;
		}

		ImVec4 GetBarColor()
		{
			return barColor;
		}
		void SetBarColor(float r, float g, float b)
		{
			barColor.x = r;
			barColor.y = g;
			barColor.z = b;
		}

		bool GetUseColorsFromImage() { return useColorsFromImage; }
		void SetUseColorsFromImage(bool useColors) { useColorsFromImage = useColors; }

		bool showCounter = true;
		bool loopColors = true;
	private:
		int width = 1280;
		int height = 720;
		int fpsLimit = 120;
		double renderInterval = 5.0;
		ImVec4 background = ImVec4(0.0, 0.0, 0.0, 1.0);
		ImVec4 barColor = ImVec4(0.52, 0.0, 0.0, 1.0);
		
		bool usePFAColors = false;
		bool useColorsFromImage = false;
		
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