#include "MIDIPlayerConfig.h"
#include <yaml-cpp/yaml.h>
#include "ConfigSection.h"
#include <optional>
#include <fstream>
#include <iostream>
#include "Utils.h"
#include <filesystem>

void MIDIPlayerConfig::LoadConfigOrDefault()
{
	std::optional<ConfigSection> config;
	try
	{
		std::ifstream stream("./config.yml", std::ios::in);
		if (!stream.is_open())
		{
			std::cout << "Failed to load config.yml. Using default config" << std::endl;
			return;
		}

		config = ConfigSection(YAML::Load(stream));

		std::optional<ConfigSection> overlaySec = config->GetSection("overlay");
		if (overlaySec)
		{
			ConfigOverlayInfo overlay;
			overlay.scale = overlaySec->GetFloat("scale", 1.0f);

			this->overlayInfo = overlay;
		}

		std::optional<ConfigSection> midiSec = config->GetSection("midi");
		if (midiSec)
		{
			ConfigMIDI midi;
			midi.multithreadedLoading = midiSec->GetBoolean("multithreaded", false);
			midi.timeBasedLoading = midiSec->GetBoolean("timeBased", false);

			this->midi = midi;
		}
		
		std::optional<ConfigSection> renderSec = config->GetSection("render");
		if (renderSec)
		{
			ConfigRender render;
			render.SetWidth(renderSec->GetInt("width", 1280));
			render.SetHeight(renderSec->GetInt("height", 720));

			auto bgCol = renderSec->GetColor("background");
			if (bgCol) render.SetBackground(bgCol->x, bgCol->y, bgCol->z);

			auto barCol = renderSec->GetColor("bar");
			if (barCol) render.SetBarColor(barCol->x, barCol->y, barCol->z);

			render.SetUseColorsFromImage(renderSec->GetBoolean("useImageColors", false));
			render.loopColors = renderSec->GetBoolean("loopColors", true);
			render.paletteID = renderSec->GetInt("paletteID", 0);

			this->render = render;
		}
	}
	catch (...)
	{
		std::cout << "Failed to load config (config.yaml). Using a default config." << std::endl;
	}
}

void MIDIPlayerConfig::SaveConfig()
{
	YAML::Node config;
	config["overlay"]["scale"] = overlayInfo.scale;

	config["midi"]["multithreaded"] = midi.multithreadedLoading;
	config["midi"]["timeBased"] = midi.timeBasedLoading;

	config["render"]["width"] = render.GetWidth();
	config["render"]["height"] = render.GetHeight();
	config["render"]["background"] = Utils::EncodeColor(render.GetBackground());
	config["render"]["bar"] = Utils::EncodeColor(render.GetBarColor());
	config["render"]["useImageColors"] = render.GetUseColorsFromImage();
	config["render"]["loopColors"] = render.loopColors;
	config["render"]["paletteID"] = render.paletteID;

	std::ofstream file("./config.yml");
	file << config;
}