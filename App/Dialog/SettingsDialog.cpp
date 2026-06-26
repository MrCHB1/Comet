#include "SettingsDialog.h"
#include "Utils.h"
#include "Render/MIDIRendererEnhanced.h"

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
			if (ImGui::BeginTabItem("Note Colors"))
			{
				ColorPaletteList* colorList = app->GetColorList();
				auto* config = app->GetConfig();

				if (colorList != nullptr)
				{
					bool setColorsFromPack = false;
					bool setColorsFromList = false;

					ImGui::TextUnformatted("Use colors from");
					ImGui::SameLine();

					if (ImGui::RadioButton("Resource pack", !config->render.GetUseColorsFromImage()))
					{
						config->render.SetUseColorsFromImage(false);
						setColorsFromPack = true;
					}

					ImGui::SameLine();
					if (ImGui::RadioButton("Color list", config->render.GetUseColorsFromImage()))
					{
						config->render.SetUseColorsFromImage(true);
						setColorsFromList = true;
					}

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

					ImGui::Spacing();
					ImGui::SetWindowFontScale(1.2f);
					ImGui::Text("Palette list");
					ImGui::Spacing();
					ImGui::SetWindowFontScale(1.0f);

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
					ImGui::Text("Loop colors");
					ImGui::SameLine();
					if (ImGui::Checkbox("##loopColors", &config->render.loopColors))
					{
						auto& currPalette = colorList->GetCurrentPalette();
						ColorAsset& colorAsset = app->GetRenderer()->GetColorAsset();
						colorAsset.LoadColors(currPalette.palette, config->render.loopColors);
					}
					ImGui::EndDisabled();
				}

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Note Counter"))
			{
				MIDIPlayerConfig* config = app->GetConfig();
				NoteCounterRenderer* counterRenderer = app->GetNoteCounterRenderer();
				
				ImGui::SetWindowFontScale(1.2f);
				ImGui::Text("Appearance");
				ImGui::Spacing();
				ImGui::SetWindowFontScale(1.0f);

				ImGui::Text("Show note counter");
				ImGui::SameLine();
				ImGui::Checkbox("##showNoteCounter", &config->render.showCounter);

				ImGui::Text("Scale");
				ImGui::SameLine();
				ImGui::SliderFloat("##counterScale", &config->overlayInfo.scale, config->overlayInfo.MIN_SCALE, config->overlayInfo.MAX_SCALE);

				ImGui::Text("Alignment");
				auto& alignment = counterRenderer->GetCounterAlignment();

				ImGui::SameLine();
				if (ImGui::RadioButton("Top Left", alignment == NoteCounterAlignment::TopLeft))
					counterRenderer->SetCounterAlignment(NoteCounterAlignment::TopLeft);

				ImGui::SameLine();
				if (ImGui::RadioButton("Top Right", alignment == NoteCounterAlignment::TopRight))
					counterRenderer->SetCounterAlignment(NoteCounterAlignment::TopRight);


				ImGui::Text("Background color");
				ImGui::SameLine();
				std::array<float, 4> bgCol = counterRenderer->GetCounterBackground();
				if (ImGui::ColorEdit4("##ncBg", bgCol.data()))
				{
					counterRenderer->SetCounterBackground(bgCol[0], bgCol[1], bgCol[2], bgCol[3]);
				}

				ImGui::Text("Text color");
				ImGui::SameLine();
				std::array<float, 3> txtCol = counterRenderer->GetCounterTextColor();
				if (ImGui::ColorEdit3("##ncTxtCol", txtCol.data()))
				{
					counterRenderer->SetCounterTextColor(txtCol[0], txtCol[1], txtCol[2]);
				}
					
				ImGui::Separator();
				ImGui::SetWindowFontScale(1.2f);
				ImGui::Text("Fields");
				ImGui::SetWindowFontScale(1.0f);
				ImGui::Text("Fields marked with an asterisk (*) means they're omitted from renders.");
				ImGui::Spacing();

				NoteCounterInfo* counter = app->GetNoteCounterInfo();
				ImGui::Checkbox("Tick", &counter->tick.shown);
				ImGui::Checkbox("Time", &counter->timeSeconds.shown);
				ImGui::Checkbox("BPM", &counter->bpm.shown);
				ImGui::Checkbox("Notes", &counter->notesPassed.shown);
				ImGui::Checkbox("NPS", &counter->notesPerSecond.shown);
				ImGui::Checkbox("Polyphony", &counter->polyphony.shown);
				ImGui::Checkbox("FPS*", &counter->fps.shown);

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Renderer"))
			{
				MIDIPlayerConfig* config = app->GetConfig();

				ImGui::Text("Current Renderer");
				RendererType currRenderer = config->render.GetCurrentRenderer();
				if (ImGui::RadioButton("Default Textured", currRenderer == RendererType::Default))
				{
					if (currRenderer != RendererType::Default)
					{
						config->render.SetCurrentRenderer(RendererType::Default);
						app->SetRenderer<MIDIRenderer>();
						std::cout << "Switched to the default renderer" << std::endl;
					}
				}
				if (ImGui::RadioButton("Enhanced Graphics", currRenderer == RendererType::Enhanced))
				{
					if (currRenderer != RendererType::Enhanced)
					{
						config->render.SetCurrentRenderer(RendererType::Enhanced);
						app->SetRenderer<MIDIRendererEnhanced>();
						std::cout << "Switched to the enhanced graphics renderer" << std::endl;
					}
				}

				ImGui::BeginDisabled(true);
				if (ImGui::RadioButton("MIDITrail (Unavailable)", currRenderer == RendererType::MIDITrail))
				{
					config->render.SetCurrentRenderer(RendererType::MIDITrail);
				}
				ImGui::EndDisabled();

				ImGui::Spacing();

				if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					app->GetRenderer()->RenderSettings();
				}

				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

void SettingsDialog::DrawMIDITab()
{
	auto* config = app->GetConfig();
	ImGui::Text("MIDI loading threads");

	ImGui::SameLine();
	if (ImGui::RadioButton("Single-threaded", !config->midi.multithreadedLoading))
		config->midi.multithreadedLoading = false;

	ImGui::SameLine();
	if (ImGui::RadioButton("Multi-threaded", config->midi.multithreadedLoading))
		config->midi.multithreadedLoading = true;

	ImGui::Text("Time-based loading");
	ImGui::SameLine();
	ImGui::Checkbox("##timebasedLoading", &config->midi.timeBasedLoading);
}