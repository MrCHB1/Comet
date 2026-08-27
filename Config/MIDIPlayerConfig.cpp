#include "MIDIPlayerConfig.h"
#include <yaml-cpp/yaml.h>
#include "ConfigSection.h"
#include <optional>
#include <fstream>
#include <iostream>
#include "Utils.h"
#include <filesystem>

#define LOAD_VAL(n, key, target) \
        if (n && n[key]) { target = n[key].as<std::decay_t<decltype(target)>>(); }

#define LOAD_COLOR(n, key, var, def) \
		uint32_t col = Utils::PackRGBA(def.x, def.y, def.z, def.w, Utils::ARGB); \
		LOAD_VAL(n, key, col); \
		auto var = Utils::UnpackRGBA(col, Utils::ARGB);

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

		YAML::Node root = YAML::Load(stream);
		config = ConfigSection(root);

		std::optional<ConfigSection> appSec = config->GetSection("app");
		
		if (appSec)
		{
			ConfigApp app;
			app.currThemeID = appSec->GetInt("themeID", 0);
			this->app = app;
		}

		std::optional<ConfigSection> overlaySec = config->GetSection("overlay");
		if (overlaySec)
		{
			ConfigOverlayInfo overlay;
			overlay.scale = overlaySec->GetFloat("scale", 1.0f);
			overlay.blurBehind = overlaySec->GetBoolean("blurBehind", true);
			overlay.selectedFontPath = overlaySec->GetString("fontPath", "");

			{
				LOAD_COLOR(root["overlay"], "background", bgColRgba, overlay.backgroundCol);
				overlay.backgroundCol = ImVec4(bgColRgba[0], bgColRgba[1], bgColRgba[2], bgColRgba[3]);
			}

			{
				LOAD_COLOR(root["overlay"], "text", txtColRgba, overlay.textCol);
				overlay.textCol = ImVec4(txtColRgba[0], txtColRgba[1], txtColRgba[2], 1.0);
			}

			overlay.overlayStyle = static_cast<NoteCounterStyle>(overlaySec->GetInt("style", static_cast<int>(DEFAULT_NOTE_COUNTER_STYLE)));
			overlay.overlayAlignment = static_cast<NoteCounterAlignment>(overlaySec->GetInt("alignment", static_cast<int>(DEFAULT_NOTE_COUNTER_ALIGNMENT)));

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

		std::optional<ConfigSection> navigationSec = config->GetSection("navigation");
		if (navigationSec)
		{
			ConfigNavigation navigation;
			navigation.alwaysHideBar = navigationSec->GetBoolean("alwaysHideBar", false);
			navigation.seekForwardSeconds = navigationSec->GetFloat("seekForwardSeconds", 10.0f);
			navigation.seekBackwardSeconds = navigationSec->GetFloat("seekBackwardSeconds", 10.0f);

			this->navigation = navigation;
		}
		
		std::optional<ConfigSection> renderSec = config->GetSection("render");
		if (renderSec)
		{
			ConfigRender render;
			render.SetWidth(renderSec->GetInt("width", 1280));
			render.SetHeight(renderSec->GetInt("height", 720));

			render.SetFPSLimit(renderSec->GetInt("fpsLimit", 120));
			render.SetVSync(renderSec->GetBoolean("vsync", true));

			{
				LOAD_COLOR(root["render"], "background", bgColRgba, render.GetBackground());
				render.SetBackground(bgColRgba[0], bgColRgba[1], bgColRgba[2]);
			}

			{
				LOAD_COLOR(root["render"], "bar", barColRgba, render.GetBarColor());
				render.SetBarColor(barColRgba[0], barColRgba[1], barColRgba[2]);
			}

			render.SetUseColorsFromImage(renderSec->GetBoolean("useImageColors", false));
			render.loopColors = renderSec->GetBoolean("loopColors", true);
			render.paletteID = renderSec->GetInt("paletteID", 0);

			render.SetCurrentRenderer(static_cast<RendererType>(renderSec->GetInt("rendererType", 0)));

			std::optional<ConfigSection> keyRangeSec = renderSec->GetSection("keyRange");
			if (keyRangeSec)
			{
				render.SetKeyFirst(keyRangeSec->GetInt("min", 0));
				render.SetKeyLast(keyRangeSec->GetInt("max", 127));
			}

			render.showCounter = renderSec->GetBoolean("showCounter", true);
			render.showStats = renderSec->GetBoolean("showStats", true);

			this->render = render;
		}

		if (root["audio"])
		{
			audioSettings = root["audio"];
		}
		
		if (root["renderers"])
		{
			renderersSettings = root["renderers"];
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
	config["version"] = 2;

	config["app"]["themeID"] = app.currThemeID;

	config["overlay"]["scale"] = overlayInfo.scale;
	config["overlay"]["blurBehind"] = overlayInfo.blurBehind;
	config["overlay"]["fontPath"] = overlayInfo.selectedFontPath;
	config["overlay"]["background"] = Utils::EncodeColor(overlayInfo.backgroundCol);
	config["overlay"]["text"] = Utils::EncodeColor(overlayInfo.textCol);
	config["overlay"]["style"] = static_cast<int>(overlayInfo.overlayStyle);
	config["overlay"]["alignment"] = static_cast<int>(overlayInfo.overlayAlignment);

	config["midi"]["multithreaded"] = midi.multithreadedLoading;
	config["midi"]["timeBased"] = midi.timeBasedLoading;

	config["navigation"]["alwaysHideBar"] = navigation.alwaysHideBar;
	config["navigation"]["seekForwardSeconds"] = navigation.seekForwardSeconds;
	config["navigation"]["seekBackwardSeconds"] = navigation.seekBackwardSeconds;

	config["render"]["width"] = render.GetWidth();
	config["render"]["height"] = render.GetHeight();
	config["render"]["fpsLimit"] = render.GetFPSLimit();
	config["render"]["vsync"] = render.GetVSync();
	config["render"]["background"] = Utils::EncodeColor(render.GetBackground());
	config["render"]["bar"] = Utils::EncodeColor(render.GetBarColor());
	config["render"]["useImageColors"] = render.GetUseColorsFromImage();
	config["render"]["loopColors"] = render.loopColors;
	config["render"]["paletteID"] = render.paletteID;
	config["render"]["rendererType"] = static_cast<int>(render.GetCurrentRenderer());
	config["render"]["keyRange"]["min"] = render.GetKeyFirst();
	config["render"]["keyRange"]["max"] = render.GetKeyLast();

	config["render"]["showCounter"] = render.showCounter;
	config["render"]["showStats"] = render.showStats;

	if (audioSettings.IsDefined())
	{
		config["audio"] = audioSettings;
	}

	if (renderersSettings.IsDefined())
	{
		config["renderers"] = renderersSettings;
	}

	std::ofstream file("./config.yml");
	file << config;
}