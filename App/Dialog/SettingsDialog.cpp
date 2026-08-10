#include "SettingsDialog.h"
#include "Utils.h"
#include "Render/MIDIRendererEnhanced.h"
#include "Render/MIDIRendererMIDITrail.h"
#include "Render/MIDIRendererPFA.h"
#include "Render/MIDIRendererChannels.h"
#include "Render/MIDIRendererVelocities.h"
#include "DialogMacros.h"

void SettingsDialog::DrawContent()
{
	if (ImGui::BeginTabBar("MainTabs"))
	{
		if (ImGui::BeginTabItem("App"))
		{
			DrawAppTab();

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Visual"))
		{
			DrawVisualTab();

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			DrawAudioTab();

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("MIDI"))
		{
			DrawMIDITab();

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::Separator();
	if (ImGui::Button("Apply & Close"))
	{
		ImGui::CloseCurrentPopup();
	}
}

void SettingsDialog::DrawAppTab()
{
	auto config = app->GetConfig();
	ThemesList* themesList = app->GetThemeList();

	if (ImGui::BeginChild("##scrollArea", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (ImGui::BeginTabBar("AppTabs"))
		{
			if (ImGui::BeginTabItem("General"))
			{
				SECTION_HEADER("Rendering");

				BEGIN_SECTION("##renderSec")
				{
					SETUP_SECTION;
					SECTION_ENTRY(SECTION_LABEL("VSync"),
					{
						bool vsync = config->render.GetVSync();
						if (ImGui::Checkbox("##vsync", &vsync))
						{
							config->render.SetVSync(vsync);
							glfwSwapInterval(vsync);
						}
					});

					SECTION_ENTRY({ SECTION_LABEL("FPS Limit"); ImGui::SetItemTooltip("When set to 0 then the FPS will be uncapped"); },
						{
							int fpsLimit = config->render.GetFPSLimit();
							if (ImGui::InputInt("##fpsLimit", &fpsLimit))
							{
								config->render.SetFPSLimit(fpsLimit);
							}
						});
					END_SECTION;
				}

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Theme"))
			{
				size_t themeIdx = 0;
				if (ImGui::Button("Reload theme list"))
				{
					themesList->ReloadThemesList();
				}
				ImGui::SameLine();
				if (ImGui::Button("Open themes folder"))
				{
					themesList->OpenThemeListFolder();
				}
				ImGui::Separator();

				for (const auto& theme : themesList->GetThemesList())
				{
					AppTheme* themePtr = theme.get();
					bool isSelected = themePtr == themesList->GetCurrentTheme();
					ImGui::PushID(themePtr);

					float width = ImGui::GetContentRegionAvail().x;
					float height = 50.0f;
					ImVec2 pos = ImGui::GetCursorScreenPos();

					if (ImGui::Selectable("##themeBtn", isSelected, 0, ImVec2(width, height)))
					{
						app->GetThemeList()->SetThemeAndApply(themeIdx);

						ThemeInfo& themeInfo = themePtr->info;
						std::cout << "Loaded \"" << themeInfo.name << "\"" << std::endl
							<< "  By " << themeInfo.author << std::endl
							<< "  Desc:" << themeInfo.description << std::endl
							<< "  Version: " << themeInfo.version << std::endl;

						config->app.currThemeID = themeIdx;
					}

					bool isHovered = ImGui::IsItemHovered();
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					// draw card background
					auto toU32 = [](const ImVec4& color) { return ImGui::ColorConvertFloat4ToU32(color); };

					ImVec4 selectColor = ImGui::GetStyleColorVec4(ImGuiCol_TabSelectedOverline);
					selectColor.w = 20;
					ImU32 bgColor = isSelected ? toU32(selectColor) : (isHovered ? IM_COL32(150, 150, 150, 30) : IM_COL32(0, 0, 0, 0));
					drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(isSelected ? ImGuiCol_CheckMark : ImGuiCol_Border), 4.0f, 0, 2.0f);

					// draw swatch previews for controls n stuff
					float swatchSize = 26.0f;
					float padding = 12.0f;
					ImVec2 swatchPos = ImVec2(pos.x + padding, pos.y + (height - swatchSize) * 0.5f);

					ImU32 outlineColor = IM_COL32(50, 50, 50, 100);

					// bg swatch
					drawList->AddRectFilled(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), toU32(theme->colors.background), 4.0f);
					drawList->AddRect(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), outlineColor, 4.0f);
					swatchPos.x += swatchSize + 4;

					// control swatch
					drawList->AddRectFilled(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), toU32(theme->colors.controlBase), 4.0f);
					drawList->AddRect(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), outlineColor, 4.0f);
					swatchPos.x += swatchSize + 4;

					// accent swatch
					drawList->AddRectFilled(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), toU32(theme->colors.accent), 4.0f);
					drawList->AddRect(swatchPos, ImVec2(swatchPos.x + swatchSize, swatchPos.y + swatchSize), outlineColor, 4.0f);
					swatchPos.x += swatchSize + 4;

					// draw them's name and author
					ImVec2 textPos = ImVec2(swatchPos.x + 15.0f, pos.y + 8.0f);
					drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), theme->info.name.c_str());

					ImVec2 subTextPos = ImVec2(textPos.x, textPos.y + ImGui::GetTextLineHeight() + 2.0f);
					drawList->AddText(subTextPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), ("by " + theme->info.author).c_str());

					ImGui::PopID();
					themeIdx++;
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Navigation"))
			{
				BEGIN_SECTION("##navi")
				{
					SETUP_SECTION;
					
					SECTION_ENTRY(
						TABLE_LABEL_TOOLTIP(
							"Always hide navigation bar",
							"Enabling this will always hide the menu/navigation bar, unless the mouse is near it."
						),
						ImGui::Checkbox("##hideNavigationbar", &config->navigation.alwaysHideBar);
					);

					SECTION_ENTRY(
						SECTION_LABEL("Jump forward secs"),
						{
							float jumpForwardSecs = config->navigation.seekForwardSeconds;
							if (ImGui::InputFloat("##forwardSecs", &jumpForwardSecs, ImGuiInputTextFlags_NoHorizontalScroll))
							{
								config->navigation.seekForwardSeconds = std::clamp(jumpForwardSecs, 0.0001f, 10.0f);
							}
						}
					);

					SECTION_ENTRY(
						SECTION_LABEL("Jump backward secs"),
						{
							float jumpBackwardSecs = config->navigation.seekBackwardSeconds;
							if (ImGui::InputFloat("##backwardSecs", &jumpBackwardSecs, ImGuiInputTextFlags_NoHorizontalScroll))
							{
								config->navigation.seekBackwardSeconds = std::clamp(jumpBackwardSecs, 0.0001f, 10.0f);
							}
						}
					);

					END_SECTION;
				}

				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::EndChild();
}

void SettingsDialog::DrawVisualTab()
{
	auto config = app->GetConfig();

	ImGui::Text("These settings will also apply when rendering videos!");
	ImGui::Separator();

	if (ImGui::BeginChild("##scrollArea", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (ImGui::BeginTabBar("VisualTabs"))
		{
			if (ImGui::BeginTabItem("Renderer"))
			{
				MIDIPlayerConfig* config = app->GetConfig();
				RendererType currRenderer = config->render.GetCurrentRenderer();

				BEGIN_SECTION("##renderer")
				{
					SETUP_SECTION;

					#define RENDERER_COMBO_ENTRY(enumTarget, target) \
						if (ImGui::Selectable(ToString(enumTarget).c_str(), currRenderer == enumTarget)) \
						{ \
							if (currRenderer != enumTarget) \
							{ \
								config->render.SetCurrentRenderer(enumTarget); \
								app->SetRenderer<target>(); \
							} \
						}

					SECTION_ENTRY(
						SECTION_LABEL("Renderer"),
						{
							std::string currentRenderer = ToString(currRenderer);
							if (ImGui::BeginCombo("##rendererCombo", currentRenderer.c_str()))
							{
								
								RENDERER_COMBO_ENTRY(RendererType::PFA, MIDIRendererPFA);
								RENDERER_COMBO_ENTRY(RendererType::Textured, MIDIRenderer);
								RENDERER_COMBO_ENTRY(RendererType::Enhanced, MIDIRendererEnhanced);
								RENDERER_COMBO_ENTRY(RendererType::MIDITrail, MIDIRendererMIDITrail);
								RENDERER_COMBO_ENTRY(RendererType::Channels, MIDIRendererChannels);
								RENDERER_COMBO_ENTRY(RendererType::Velocities, MIDIRendererVelocities);

								ImGui::EndCombo();
							}
						}
					);

					#undef RENDERER_COMBO_ENTRY

					END_SECTION;
				}


				ImGui::Spacing();

				if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					app->GetRenderer()->RenderSettings();
				}

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Note Colors"))
			{
				ColorPaletteList* colorList = app->GetColorList();
				auto* config = app->GetConfig();

				if (colorList != nullptr)
				{
					bool setColorsFromPack = false;
					bool setColorsFromList = false;

					BEGIN_SECTION("##noteColors")
					{
						SETUP_SECTION;

						SECTION_ENTRY(
							SECTION_LABEL("Use colors from"),
							if (ImGui::RadioButton("Resource pack", !config->render.GetUseColorsFromImage()))
							{
								config->render.SetUseColorsFromImage(false);
								setColorsFromPack = true;
							}

							if (ImGui::RadioButton("Color list", config->render.GetUseColorsFromImage()))
							{
								config->render.SetUseColorsFromImage(true);
								setColorsFromList = true;
							}
						);

						if (setColorsFromList)
						{
							auto& entry = colorList->GetCurrentPalette();
							app->GetRenderer()->LoadColors(entry.palette);
						}
						else if (setColorsFromPack)
						{
							ResourcePackList* packList = app->GetPackList();
							auto currPack = packList->GetActivePack();
							auto noteColors = currPack->GetStream("noteColors.png");

							auto& colorAsset = app->GetRenderer()->GetColorAsset();
							if (noteColors != nullptr)
							{
								colorAsset.LoadColors(noteColors, currPack->GetNoteInfo()->loopColors);
							}
							else
							{
								colorAsset.ResetColors();
								colorAsset.LoadColors();
							}
						}

						END_SECTION;
					}

					SECTION_HEADER("Palette list");

					ImGui::BeginDisabled(!config->render.GetUseColorsFromImage());
					if (ImGui::Button("Refresh palette list"))
					{
						colorList->ReloadList();
						auto& entry = colorList->GetCurrentPalette();
						app->GetRenderer()->LoadColors(entry.palette);
					}
					ImGui::SameLine();
					if (ImGui::Button("Open palette folder"))
					{
						Utils::OpenFolder("./colors");
					}

					ImGui::Spacing();

					auto& palettes = colorList->GetPalettes();
					const ColorPaletteEntry& currPalette = colorList->GetCurrentPalette();

					ImGui::BeginChild("PaletteScroll", ImVec2(0, 260), true, ImGuiWindowFlags_HorizontalScrollbar);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
					
					for (size_t i = 0; i < palettes.size(); ++i)
					{
						const auto& palette = palettes[i];
						bool isActivePalette = (&palette == &currPalette);

						ImGui::PushID((int)i);
						ImGui::BeginGroup();

						ImVec2 cardSize(ImGui::GetContentRegionAvail().x, 62.0f);
						if (ImGui::Selectable("##palette_card", isActivePalette,
							0, cardSize))
						{
							colorList->SetPalette(i);
							config->render.paletteID = i;
							app->GetRenderer()->GetColorAsset().LoadColors(palette.palette, true);
						}

						ImDrawList* dl = ImGui::GetWindowDrawList();
						ImVec2 min = ImGui::GetItemRectMin();
						ImVec2 max = ImGui::GetItemRectMax();

						ImU32 bg = isActivePalette
							? ImGui::GetColorU32(ImGuiCol_FrameBgActive)
							: ImGui::GetColorU32(ImGuiCol_FrameBg);

						dl->AddRect(min, max, ImGui::GetColorU32(isActivePalette ? ImGuiCol_CheckMark : ImGuiCol_Border), 4.0f, 0, 2.0f);

						// Palette name
						ImVec2 textPos(min.x + 12.0f, min.y + 10.0f);
						dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), palette.name.c_str());

						// Small color preview strip
						float swatchY = min.y + 34.0f;
						float swatchW = 26.0f;
						float swatchH = 14.0f;
						float swatchX = min.x + 12.0f;
						const int previewCount = (int)std::min<size_t>(10, palette.palette.size());

						for (int s = 0; s < previewCount; ++s)
						{
							const auto& c = palette.palette[s];
							ImU32 col = IM_COL32(c[0] * 255, c[1] * 255, c[2] * 255, 255);

							ImVec2 a(swatchX + s * (swatchW + 4.0f), swatchY);
							ImVec2 b(a.x + swatchW, a.y + swatchH);
							dl->AddRectFilled(a, b, col, 4.0f);
							dl->AddRect(a, b, IM_COL32(0, 0, 0, 60), 4.0f);
						}

						if (isActivePalette)
						{
							ImVec2 badgePos(max.x - 72.0f, min.y + 10.0f);
							dl->AddText(badgePos, ImGui::GetColorU32(ImGuiCol_CheckMark), "In use");
						}

						ImGui::EndGroup();
						ImGui::PopID();
					}

					ImGui::PopStyleVar();
					ImGui::EndChild();

					BEGIN_SECTION("##noteColorsEnd")
					{
						SETUP_SECTION;

						SECTION_ENTRY(
							SECTION_LABEL("Loop colors"),
							if (ImGui::Checkbox("##loopColors", &config->render.loopColors))
							{
								auto& currPalette = colorList->GetCurrentPalette();
								ColorAsset& colorAsset = app->GetRenderer()->GetColorAsset();
								colorAsset.LoadColors(currPalette.palette, config->render.loopColors);
							}
						);

						END_SECTION;
					}
					ImGui::EndDisabled();
				}

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Note Counter"))
			{
				MIDIPlayerConfig* config = app->GetConfig();
				NoteCounterRenderer* counterRenderer = app->GetNoteCounterRenderer();
				NoteCounterInfo* counter = app->GetNoteCounterInfo();
				FontList* fontList = app->GetFontList();
				std::vector<FontEntry>& fonts = fontList->GetFonts();

				SECTION_HEADER("Appearance");

				BEGIN_SECTION("##nc_appearance")
				{
					SETUP_SECTION;

					SECTION_ENTRY(
						SECTION_LABEL("Show note counter"),
						ImGui::Checkbox("##showNoteCounter", &config->render.showCounter);
					);

					SECTION_ENTRY(
						SECTION_LABEL("Scale"),
						{
							ImGui::SetNextItemWidth(-FLT_MIN);
							ImGui::SliderFloat(
								"##counterScale",
								&config->overlayInfo.scale,
								config->overlayInfo.MIN_SCALE,
								config->overlayInfo.MAX_SCALE
							);
						}
					);

					SECTION_ENTRY(
						SECTION_LABEL("Alignment"),
						{
							const auto alignment = counterRenderer->GetCounterAlignment();

							if (ImGui::RadioButton(
								"Top Left",
								alignment == NoteCounterAlignment::TopLeft))
							{
								counterRenderer->SetCounterAlignment(
									NoteCounterAlignment::TopLeft
								);
							}

							ImGui::BeginDisabled(
								counterRenderer->GetCounterStyle() != NoteCounterStyle::MIDITrail
							);

							ImGui::SameLine();

							if (ImGui::RadioButton(
								"Top Center",
								alignment == NoteCounterAlignment::TopCenter))
							{
								counterRenderer->SetCounterAlignment(
									NoteCounterAlignment::TopCenter
								);
							}

							ImGui::EndDisabled();

							ImGui::SameLine();

							if (ImGui::RadioButton(
								"Top Right",
								alignment == NoteCounterAlignment::TopRight))
							{
								counterRenderer->SetCounterAlignment(
									NoteCounterAlignment::TopRight
								);
							}
						}
					);

					SECTION_ENTRY(
						SECTION_LABEL("Blur behind"),
						ImGui::Checkbox(
							"##blurBehind",
							&config->overlayInfo.blurBehind
						);
					);

					SECTION_ENTRY(
						SECTION_LABEL("Font"),
						{
							std::string currentFontName = "Default";

							for (const auto& f : fonts)
							{
								if (f.path == config->overlayInfo.selectedFontPath)
								{
									currentFontName = f.name;
									break;
								}
							}

							if (ImGui::BeginCombo(
								"##counterFontCombo",
								currentFontName.c_str()))
							{
								if (ImGui::Selectable(
									"Default",
									config->overlayInfo.selectedFontPath.empty()))
								{
									config->overlayInfo.selectedFontPath.clear();
								}

								for (const auto& f : fonts)
								{
									const bool isSelected =
										config->overlayInfo.selectedFontPath == f.path;

									if (ImGui::Selectable(
										f.name.c_str(),
										isSelected))
									{
										config->overlayInfo.selectedFontPath = f.path;
									}

									if (isSelected)
										ImGui::SetItemDefaultFocus();
								}

								ImGui::EndCombo();
							}
						}
					);

					END_SECTION;
				}


				SECTION_HEADER("Style");

				BEGIN_SECTION("##nc_style")
				{
					SETUP_SECTION;

					SECTION_ENTRY(
						SECTION_LABEL("Style"),
						{
							const auto style = counterRenderer->GetCounterStyle();

							if (ImGui::RadioButton(
								"Ultralight MIDI Player",
								style == NoteCounterStyle::UMP))
							{
								counterRenderer->SetCounterStyle(NoteCounterStyle::UMP);

								if (counterRenderer->GetCounterAlignment() ==
									NoteCounterAlignment::TopCenter)
								{
									counterRenderer->SetCounterAlignment(
										NoteCounterAlignment::TopLeft
									);
								}
							}

							ImGui::SameLine();

							if (ImGui::RadioButton(
								"MIDITrail",
								style == NoteCounterStyle::MIDITrail))
							{
								counterRenderer->SetCounterStyle(
									NoteCounterStyle::MIDITrail
								);
							}
						}
					);

					SECTION_ENTRY(
						SECTION_LABEL("Background color"),
						{
							std::array<float, 4> bgCol =
								counterRenderer->GetCounterBackground();

							if (ImGui::ColorEdit4(
								"##ncBg",
								bgCol.data(),
								ImGuiColorEditFlags_AlphaBar))
							{
								counterRenderer->SetCounterBackground(
									bgCol[0],
									bgCol[1],
									bgCol[2],
									bgCol[3]
								);
							}
						}
					);

					SECTION_ENTRY(
						SECTION_LABEL("Text color"),
						{
							std::array<float, 3> txtCol =
								counterRenderer->GetCounterTextColor();

							if (ImGui::ColorEdit3(
								"##ncTxtCol",
								txtCol.data()))
							{
								counterRenderer->SetCounterTextColor(
									txtCol[0],
									txtCol[1],
									txtCol[2]
								);
							}
						}
					);

					END_SECTION;
				}

				SECTION_HEADER("Fields");
				ImGui::TextDisabled("Fields marked with an asterisk (*) are omitted from renders.");
				ImGui::Spacing();

				BEGIN_SECTION("##nc_fields")
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Checkbox("Tick", &counter->tick.shown);
					ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Time", &counter->timeSeconds.shown);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Checkbox("BPM", &counter->bpm.shown);
					ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Notes", &counter->notesPassed.shown);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Checkbox("NPS", &counter->notesPerSecond.shown);
					ImGui::TableSetColumnIndex(1); ImGui::Checkbox("Polyphony", &counter->polyphony.shown);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Checkbox("FPS*", &counter->fps.shown);
					ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("");

					END_SECTION;
				}

				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

void SettingsDialog::DrawAudioTab()
{
	MIDIAudio* midiAudio = app->GetMIDIAudio();

	// ImGui::Text("Audio Engine");
	BEGIN_SECTION("##audioEngine")
	{
		SETUP_SECTION;

		SECTION_ENTRY(
			SECTION_LABEL("Audio engine"),
			{
				AudioEngineList & engineList = midiAudio->GetEngineList();
				size_t engineIndex = 0;
				for (auto& engine : engineList)
				{
					if (engine == nullptr)
					{
						engineIndex++;
						continue;
					}

					AudioEngineType type = (AudioEngineType)engineIndex;

					bool isSupported = engine->IsSupported();

					ImGui::BeginDisabled(!isSupported);
					if (ImGui::RadioButton(engine->GetName().c_str(), midiAudio->GetCurrentEngineType() == type))
						midiAudio->SwitchEngine(type);

					if (!isSupported)
						ImGui::SetItemTooltip("This engine is not supported on your platform.");
					ImGui::EndDisabled();

					ImGui::Spacing();

					engineIndex++;
				}
			}
		);

		END_SECTION;
	}
	
	if (ImGui::CollapsingHeader("Engine Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		midiAudio->GetCurrentEngine()->RenderSettings();
	}
}

void SettingsDialog::DrawMIDITab()
{
	auto* config = app->GetConfig();

	BEGIN_SECTION("##midiSettings")
	{
		SETUP_SECTION;

		SECTION_ENTRY(
			SECTION_LABEL("MIDI loading threads"),
			{
				if (ImGui::RadioButton("Single-threaded", !config->midi.multithreadedLoading))
					config->midi.multithreadedLoading = false;

				if (ImGui::RadioButton("Multi-threaded", config->midi.multithreadedLoading))
					config->midi.multithreadedLoading = true;
			}
		);

		SECTION_ENTRY(
			SECTION_LABEL("Time-based loading"),
			{ ImGui::Checkbox("##timebasedLoading", &config->midi.timeBasedLoading); }
		);

		END_SECTION;
	}
}