#pragma once

#include "imgui.h"

#include "ConfigSection.h"
#include "../MIDI/MIDIDefs.h"
#include <string>

enum class RendererType
{
	PFA,
	Synthesia,
	Textured,
	Enhanced,
	MIDITrail,
	Channels,
	Velocities,
	CounterOnly
};

inline const std::string ToString(RendererType type)
{
	switch (type)
	{
		case RendererType::PFA:         return "Piano From Above";
		case RendererType::Synthesia:   return "Synthesia";
		case RendererType::Textured:    return "Textured";
		case RendererType::Enhanced:    return "Enhanced Graphics";
		case RendererType::MIDITrail:   return "MIDITrail";
		case RendererType::Channels:    return "Channels";
		case RendererType::Velocities:  return "Velocities";
		case RendererType::CounterOnly: return "Counter Only";
	}

	return "Unknown";
}

struct MIDIPlayerConfig
{
	struct ConfigApp
	{
		size_t currThemeID = 0;
	};

	struct ConfigOverlayInfo
	{
		static constexpr float MIN_SCALE = 0.25f;
		static constexpr float MAX_SCALE = 8.0f;

		float scale = 1.0f;
		std::string selectedFontPath = "";
		bool showDuration = false;
		bool blurBehind = true;
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
	};

	struct ConfigNavigation
	{
		bool alwaysHideBar = false;
		float seekForwardSeconds = 10.0f;
		float seekBackwardSeconds = 10.0f;
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
		void SetFPSLimit(int fpsLimit)
		{
			this->fpsLimit = fpsLimit;
			if (this->fpsLimit > 240)
				this->fpsLimit = 240;
			else if (this->fpsLimit < 15 && this->fpsLimit > 0)
				this->fpsLimit = 15;
			else if (this->fpsLimit <= 0)
				this->fpsLimit = 0; // uncapped fps xd
		}
		bool GetVSync()
		{
			return vsync;
		}
		void SetVSync(bool vsync)
		{
			this->vsync = vsync;
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

		RendererType GetCurrentRenderer() { return currentRenderer; }
		void SetCurrentRenderer(RendererType renderer)
		{
			currentRenderer = renderer;
		}

		int GetKeyFirst() { return keyFirst; }
		void SetKeyFirst(int key)
		{
			if (key < 0) key = 0;
			if (key > keyLast) key = keyLast;
			
			if (keyFirst != key)
			{
				keyFirst = key;
				keyRangeChanged = true;
			}
		}

		int GetKeyLast() { return keyLast; }
		void SetKeyLast(int key)
		{
			if (key > MIDI_KEYS - 1) key = MIDI_KEYS - 1;
			if (key < keyFirst) key = keyFirst;
			
			if (keyLast != key)
			{
				keyLast = key;
				keyRangeChanged = true;
			}
		}

		bool ConsumeKeyRangeChanged()
		{
			bool tmp = keyRangeChanged;
			keyRangeChanged = false;
			return tmp;
		}

		bool showCounter = true;
		bool loopColors = true;
		int paletteID = 0;
	private:
		int width = 1280;
		int height = 720;
		int fpsLimit = 120;
		bool vsync = true;
		double renderInterval = 5.0;
		ImVec4 background = ImVec4(0.0, 0.0, 0.0, 1.0);
		ImVec4 barColor = ImVec4(0.52, 0.0, 0.0, 1.0);
		RendererType currentRenderer = RendererType::PFA;
		
		bool usePFAColors = false;
		bool useColorsFromImage = false;
		
		int keyFirst = 0;
		int keyLast = 127;
		bool keyRangeChanged = false;

		std::string resourcePack = "";
		std::string font = "Monospaced";
		MIDIPlayerConfig::ConfigOverlay overlay{};
	};

	int version = 0;
	std::string language = "en";

	ConfigApp app{};
	ConfigMIDI midi{};
	ConfigNavigation navigation{};
	ConfigRender render{};
	ConfigOverlayInfo overlayInfo{};

	YAML::Node audioSettings;
	YAML::Node renderersSettings;

	void LoadConfigOrDefault();
	void SaveConfig();
};