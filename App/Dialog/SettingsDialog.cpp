#include "SettingsDialog.h"
#include "Utils.h"
// REFACTOR (compile-memory): this dialog instantiates MIDIRenderer via
// MIDIApp::SetRenderer<MIDIRenderer>(), so it needs the complete type. The
// include used to arrive transitively through MIDIRendererMIDITrail.h, which no
// longer pulls the renderer hub.
#include "App/MIDIApp.h"
#include "Render/MIDIRenderer.h"
#include "Render/MIDIRendererPFA.h"
#include "Render/MIDIRendererSynthesia.h"
#include "Render/MIDIRendererEnhanced.h"
#include "Render/MIDIRendererMIDITrail.h"
#include "Render/MIDIRendererChannels.h"
#include "Render/MIDIRendererVelocities.h"
#include "Render/MIDIRendererCounter.h"
#include "Render/NoteCounter/NoteCounterRenderer.h"
#include "DialogMacros.h"
#include "App/UI/Themes/Themes.h"
#include "ColorPalette/ColorPaletteList.h"
#include "ResourcePack/ResourcePackList.h"
#include "App/FontList.h"
#include "MIDI/Audio/MIDIAudio.h"

#include "App/UI/Layout/ForumLayout.h"
#include "App/Localization/Localization.h"

namespace
{
	using namespace Comet::UI::Layout;

	template <typename Enum, typename DrawContentFn>
	void DrawSubTabs(const char* id, const char* heading,
		Enum& selected, float sidebarWidth,
		std::initializer_list<std::pair<std::string, Enum>> tabs,
		DrawContentFn&& drawContent)
	{
		ImGui::PushID(id);

		// Left sidebar
		ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0.0f),
			ImGuiChildFlags_Borders);

		{
			const ImVec4 headingColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

			ImGui::TextColored(
				ImVec4(headingColor.x, headingColor.y, headingColor.z, 0.5f), "%s", heading);

			ImGui::Spacing();

			for (const auto& [label, value] : tabs)
			{
				if (ImGui::Selectable(label.c_str(), selected == value)) selected = value;
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// Right content panel
		ImGui::BeginChild("Content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		{
			drawContent();
		}
		ImGui::EndChild();

		ImGui::PopID();
	}

	class IDScope
	{
	public:
		IDScope(const char* id) { ImGui::PushID(id); }
		~IDScope() { ImGui::PopID(); }

		IDScope& operator=(const IDScope&&) = delete;
	};
}

void SettingsDialog::DrawContent()
{
	Localization* localization = app->GetLocalization();

	const float footerHeight =
		ImGui::GetFrameHeightWithSpacing() + 2.0f * ImGui::GetStyle().ItemSpacing.y;

	if (ImGui::BeginChild("SettingsBody", ImVec2(0.0, -footerHeight), ImGuiChildFlags_None))
	{
		if (ImGui::BeginTabBar("MainTabs"))
		{
			{
				IDScope id("app");
				if (ImGui::BeginTabItem(TRANSLATE("dialog.settings.app").c_str()))
				{
					DrawAppTab();

					ImGui::EndTabItem();
				}
			}

			{
				IDScope id("visual");
				if (ImGui::BeginTabItem(TRANSLATE("dialog.settings.visual").c_str()))
				{
					DrawVisualTab();

					ImGui::EndTabItem();
				}
			}
			
			{
				IDScope id("audio");
				if (ImGui::BeginTabItem(TRANSLATE("dialog.settings.audio").c_str()))
				{
					DrawAudioTab();

					ImGui::EndTabItem();
				}
			}

			{
				IDScope id("midi");
				if (ImGui::BeginTabItem(TRANSLATE("dialog.settings.midi").c_str()))
				{
					DrawMIDITab();

					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}
	}

	ImGui::EndChild();
	ImGui::Separator();
	if (ImGui::Button(TRANSLATE("dialog.applyclose").c_str()))
	{
		ImGui::CloseCurrentPopup();
	}
}

void SettingsDialog::DrawAppTab()
{
	auto config = app->GetConfig();
	Localization* localization = app->GetLocalization();

	ThemesList* themesList = app->GetThemeList();

	enum AppSubTab
	{
		General,
		Theme,
		Navigation
	};

	static AppSubTab appSubTab = General;

	DrawSubTabs("AppSettings",
		TRANSLATE("dialog.settings.app.header").c_str(),
		appSubTab, 160.0f, {
			{ TRANSLATE("dialog.settings.app.general"), General },
			{ TRANSLATE("dialog.settings.app.theme"), Theme },
			{ TRANSLATE("dialog.settings.app.nav"), Navigation }
		},
		[&] {
			switch (appSubTab)
			{
			case General:
			{
				ForumLayout layout("##generalTab");

				layout.AddField("##language",
					TRANSLATE("dialog.settings.app.general.language").c_str(),
					[&]()
					{
						auto* curr = localization->GetCurrentLanguage();
						if (!curr) return;
						if (ImGui::BeginCombo("", curr->label.c_str()))
						{
							for (const auto& lang : localization->GetLanguageList())
							{
								if (ImGui::Selectable(lang.label.c_str(), lang.id == curr->id))
									app->SetLanguage(lang.id);
							}

							ImGui::EndCombo();
						}
					});

				layout.AddField("##vsync",
					TRANSLATE("dialog.settings.app.general.vsync").c_str(),
					[&]()
					{
						bool vsync = config->render.GetVSync();
						if (ImGui::Checkbox("", &vsync))
						{
							config->render.SetVSync(vsync);
							glfwSwapInterval(vsync);
						}
					});

				layout.AddFieldWithTooltip("##fpsLimit",
					TRANSLATE("dialog.settings.app.general.fps").c_str(),
					TRANSLATE("dialog.settings.app.general.fps.tooltip").c_str(),
					[&]()
					{
						int fpsLimit = config->render.GetFPSLimit();
						if (ImGui::InputInt("##fpsLimit", &fpsLimit))
						{
							config->render.SetFPSLimit(fpsLimit);
						}
					});

				break;
			}
			case Theme:
			{
				size_t themeIdx = 0;

				if (ImGui::Button(TRANSLATE("dialog.settings.app.theme.reload").c_str()))
					themesList->ReloadThemesList();

				ImGui::SameLine();

				if (ImGui::Button(TRANSLATE("dialog.settings.app.theme.open").c_str()))
					themesList->OpenThemeListFolder();

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

					// draw theme's name and author
					ImVec2 textPos = ImVec2(swatchPos.x + 15.0f, pos.y + 8.0f);
					drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), theme->info.name.c_str());

					ImVec2 subTextPos = ImVec2(textPos.x, textPos.y + ImGui::GetTextLineHeight() + 2.0f);
					drawList->AddText(subTextPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), ("by " + theme->info.author).c_str());

					ImGui::PopID();
					themeIdx++;
				}

				break;
			}
			case Navigation:
			{
				ForumLayout layout("##navigation");

				layout.AddFieldWithTooltip("##hideNavigationBar",
					TRANSLATE("dialog.settings.app.nav.hide").c_str(),
					TRANSLATE("dialog.settings.app.nav.hide.tooltip").c_str(),
					[&]()
					{
						ImGui::Checkbox("", &config->navigation.alwaysHideBar);
					});

				layout.AddField("##forwardSecs",
					TRANSLATE("dialog.settings.app.nav.forward").c_str(),
					[&]()
					{
						float jumpForwardSecs = config->navigation.seekForwardSeconds;
						if (ImGui::InputFloat("", &jumpForwardSecs, ImGuiInputTextFlags_NoHorizontalScroll))
						{
							config->navigation.seekForwardSeconds = std::clamp(jumpForwardSecs, 0.0001f, 10.0f);
						}
					});

				layout.AddField("##backwardSecs",
					TRANSLATE("dialog.settings.app.nav.backward").c_str(),
					[&]()
					{
						float jumpBackwardSecs = config->navigation.seekBackwardSeconds;
						if (ImGui::InputFloat("", &jumpBackwardSecs, ImGuiInputTextFlags_NoHorizontalScroll))
						{
							config->navigation.seekBackwardSeconds = std::clamp(jumpBackwardSecs, 0.0001f, 10.0f);
						}
					});

				break;
			}
			}
		});
}

void SettingsDialog::DrawVisualTab()
{
	auto config = app->GetConfig();
	Localization* localization = app->GetLocalization();

	ImGui::Text(TRANSLATE("dialog.settings.visual.disclaimer").c_str());
	ImGui::Separator();

	enum VisualSubTab
	{
		Renderer,
		NoteColors,
		CounterAndStats
	};

	static VisualSubTab visualSubTab = Renderer;

	DrawSubTabs("VisualSettings",
		TRANSLATE("dialog.settings.visual.header").c_str(),
		visualSubTab, 160.0f, {
			{ TRANSLATE("dialog.settings.visual.renderer"), Renderer },
			{ TRANSLATE("dialog.settings.visual.colors"), NoteColors },
			{ TRANSLATE("dialog.settings.visual.counter"), CounterAndStats }
		},
		[&] {
			switch (visualSubTab)
			{
			case Renderer:
			{
				MIDIPlayerConfig* config = app->GetConfig();
				RendererType currRenderer = config->render.GetCurrentRenderer();

				{
					ForumLayout layout("##renderer");

					#define RENDERER_COMBO_ENTRY(enumTarget, target) \
						if (ImGui::Selectable(ToString(enumTarget).c_str(), currRenderer == enumTarget)) \
						{ \
							if (currRenderer != enumTarget) \
							{ \
								config->render.SetCurrentRenderer(enumTarget); \
								app->SetRenderer<target>(); \
							} \
						}

					layout.AddField("##rendererCombo",
						TRANSLATE("dialog.settings.visual.renderer").c_str(),
						[&]()
						{
							std::string currentRenderer = ToString(currRenderer);
							const char* name = currentRenderer.c_str();
							if (ImGui::BeginCombo("", name))
							{
								RENDERER_COMBO_ENTRY(RendererType::PFA, MIDIRendererPFA);
								RENDERER_COMBO_ENTRY(RendererType::Synthesia, MIDIRendererSynthesia);
								RENDERER_COMBO_ENTRY(RendererType::Textured, MIDIRenderer);
								RENDERER_COMBO_ENTRY(RendererType::Enhanced, MIDIRendererEnhanced);
								RENDERER_COMBO_ENTRY(RendererType::MIDITrail, MIDIRendererMIDITrail);
								RENDERER_COMBO_ENTRY(RendererType::Channels, MIDIRendererChannels);
								RENDERER_COMBO_ENTRY(RendererType::Velocities, MIDIRendererVelocities);
								RENDERER_COMBO_ENTRY(RendererType::CounterOnly, MIDIRendererCounter);

								ImGui::EndCombo();
							}
						});

					#undef RENDERER_COMBO_ENTRY
				}

				ImGui::Spacing();

				if (ImGui::CollapsingHeader(
					TRANSLATE("dialog.settings.visual.renderer.settings").c_str(),
					ImGuiTreeNodeFlags_DefaultOpen))
				{
					app->GetRenderer()->RenderSettings();
				}

				break;
			}
			case NoteColors:
			{
				ColorPaletteList* colorList = app->GetColorList();
				auto* config = app->GetConfig();

				if (colorList != nullptr)
				{
					bool setColorsFromPack = false;
					bool setColorsFromList = false;

					ForumLayout layout("##noteColors");
					layout.AddField("##colorRadio",
						TRANSLATE("dialog.settings.visual.colors.use").c_str(),
						[&]()
						{
							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.colors.use.0").c_str(),
								!config->render.GetUseColorsFromImage()))
							{
								config->render.SetUseColorsFromImage(false);
								setColorsFromPack = true;
							}

							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.colors.use.1").c_str(), 
								config->render.GetUseColorsFromImage()))
							{
								config->render.SetUseColorsFromImage(true);
								setColorsFromList = true;
							}
						});

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

					ImGui::BeginDisabled(!config->render.GetUseColorsFromImage());
					layout.AddHeader(TRANSLATE("dialog.settings.visual.colors.palette").c_str());
					{
						ForumLayoutBreak break_(&layout);

						if (ImGui::Button(TRANSLATE("dialog.settings.visual.colors.refresh").c_str()))
						{
							colorList->ReloadList();
							auto& entry = colorList->GetCurrentPalette();
							app->GetRenderer()->LoadColors(entry.palette);
						}
						ImGui::SameLine();
						if (ImGui::Button(TRANSLATE("dialog.settings.visual.colors.open").c_str()))
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
					}

					layout.AddField("##loopColors",
						TRANSLATE("dialog.settings.visual.colors.loop").c_str(),
						[&]()
						{
							if (ImGui::Checkbox("", &config->render.loopColors))
							{
								auto& currPalette = colorList->GetCurrentPalette();
								ColorAsset& colorAsset = app->GetRenderer()->GetColorAsset();
								colorAsset.LoadColors(currPalette.palette, config->render.loopColors);
							}
						});

					layout.AddField("##shuffleColors",
						TRANSLATE("dialog.settings.visual.colors.shuffle").c_str(),
						[&]()
						{
							if (ImGui::Checkbox("", &config->render.shuffleColors))
							{
								ColorAsset& colorAsset = app->GetRenderer()->GetColorAsset();
								if (config->render.shuffleColors)
									colorAsset.ShuffleColors((uint32_t)config->render.shuffleSeed);
								else
									colorAsset.RestoreColors();
							}
						});

					if (config->render.shuffleColors)
					{
						layout.AddField("##shuffleSeed",
							TRANSLATE("dialog.settings.visual.colors.seed").c_str(),
							[&]()
							{
								if (ImGui::InputInt("##shuffleSeed", &config->render.shuffleSeed))
								{
									if (config->render.shuffleSeed < 0) config->render.shuffleSeed = 0;
									ColorAsset& colorAsset = app->GetRenderer()->GetColorAsset();
									if (config->render.shuffleColors) colorAsset.ShuffleColors((uint32_t)config->render.shuffleSeed);
								}
							});
					}

					ImGui::EndDisabled();
				}

				break;
			}
			case CounterAndStats:
			{
				MIDIPlayerConfig* config = app->GetConfig();
				NoteCounterInfo* counter = app->GetNoteCounterInfo();
				FontList* fontList = app->GetFontList();
				std::vector<FontEntry>& fonts = fontList->GetFonts();

				const NoteCounterAlignment alignment = config->overlayInfo.overlayAlignment;
				const NoteCounterStyle style = config->overlayInfo.overlayStyle;

				{
					ForumLayout layout("##counterSettings");
					layout.AddHeader(TRANSLATE("dialog.settings.visual.counter.appearance").c_str());

					layout.AddField("##showNoteCounter",
						TRANSLATE("dialog.settings.visual.counter.showcounter").c_str(),
						[&]()
						{
							ImGui::Checkbox("", &config->render.showCounter);
						});

					layout.AddField("##showStatistics",
						TRANSLATE("dialog.settings.visual.counter.showstats").c_str(),
						[&]()
						{
							ImGui::Checkbox("", &config->render.showStats);
						});

					layout.AddField("##scale",
						TRANSLATE("dialog.settings.visual.counter.scale").c_str(),
						[&]()
						{
							ImGui::SliderFloat("",
								&config->overlayInfo.scale,
								config->overlayInfo.MIN_SCALE,
								config->overlayInfo.MAX_SCALE
								);
						});

					layout.AddField("##alignment",
						TRANSLATE("dialog.settings.visual.counter.align").c_str(),
						[&]()
						{
							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.counter.align.0").c_str(),
								alignment == NoteCounterAlignment::TopLeft))
							{
								config->overlayInfo.overlayAlignment = NoteCounterAlignment::TopLeft;
							}

							ImGui::BeginDisabled(
								style != NoteCounterStyle::MIDITrail
							);

							ImGui::SameLine();

							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.counter.align.1").c_str(),
								alignment == NoteCounterAlignment::TopCenter))
							{
								config->overlayInfo.overlayAlignment = NoteCounterAlignment::TopCenter;
							}

							ImGui::EndDisabled();

							ImGui::SameLine();

							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.counter.align.2").c_str(),
								alignment == NoteCounterAlignment::TopRight))
							{
								config->overlayInfo.overlayAlignment = NoteCounterAlignment::TopRight;
							}
						});

					layout.AddField("##blurBehind",
						TRANSLATE("dialog.settings.visual.counter.blur").c_str(),
						[&]()
						{
							ImGui::Checkbox("", &config->overlayInfo.blurBehind);
						});

					layout.AddField("##font",
						TRANSLATE("dialog.settings.visual.counter.font").c_str(),
						[&]()
						{
							const std::string defaultEntry =
								TRANSLATE("dialog.settings.visual.counter.font.default");

							std::string currentFontName = defaultEntry;

							for (const auto& f : fonts)
							{
								if (f.path == config->overlayInfo.selectedFontPath)
								{
									currentFontName = f.name;
									break;
								}
							}

							if (ImGui::BeginCombo("##fontCombo", currentFontName.c_str()))
							{
								if (ImGui::Selectable(defaultEntry.c_str(),
									config->overlayInfo.selectedFontPath.empty()))
								{
									config->overlayInfo.selectedFontPath.clear();
								}

								for (const auto& f : fonts)
								{
									const bool isSelected = config->overlayInfo.selectedFontPath == f.path;

									if (ImGui::Selectable(f.name.c_str(), isSelected))
										config->overlayInfo.selectedFontPath = f.path;

									if (isSelected)
										ImGui::SetItemDefaultFocus();
								}

								ImGui::EndCombo();
							}
						});

					layout.AddField("##directory",
						TRANSLATE("dialog.settings.visual.counter.font.directory").c_str(),
						[&]()
						{
							if (ImGui::Button(TRANSLATE("general.open").c_str()))
								Utils::OpenFolder("./assets/fonts/");
						});

					std::string counterStyleLabel = TRANSLATE("dialog.settings.visual.counter.style");
					layout.AddHeader(counterStyleLabel.c_str());

					layout.AddField("##counterStyle", counterStyleLabel.c_str(),
						[&]()
						{
							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.counter.style.0").c_str(),
								style == NoteCounterStyle::UMP))
							{
								config->overlayInfo.overlayStyle = NoteCounterStyle::UMP;

								if (config->overlayInfo.overlayAlignment == NoteCounterAlignment::TopCenter)
								{
									config->overlayInfo.overlayAlignment = NoteCounterAlignment::TopRight;
								}
							}

							ImGui::SameLine();

							if (ImGui::RadioButton(
								TRANSLATE("dialog.settings.visual.counter.style.1").c_str(),
								style == NoteCounterStyle::MIDITrail))
							{
								config->overlayInfo.overlayStyle = NoteCounterStyle::MIDITrail;
							}
						});

					layout.AddField("##ncBg",
						TRANSLATE("dialog.settings.visual.counter.background").c_str(),
						[&]()
						{
							ImVec4 backgroundColor = config->overlayInfo.backgroundCol;
							if (ImGui::ColorEdit4("", &backgroundColor.x, ImGuiColorEditFlags_AlphaBar))
							{
								config->overlayInfo.backgroundCol = backgroundColor;
							}
						});

					layout.AddField("##ncTxt",
						TRANSLATE("dialog.settings.visual.counter.text").c_str(),
						[&]()
						{
							ImVec4 textColor = config->overlayInfo.textCol;
							if (ImGui::ColorEdit3("", &textColor.x))
							{
								config->overlayInfo.textCol = textColor;
							}
						});

					layout.AddHeader(TRANSLATE("dialog.settings.visual.counter.fields").c_str());
				}

				ImGui::TextDisabled(
					TRANSLATE("dialog.settings.visual.counter.fields.help").c_str());
				ImGui::Spacing();

				if (ImGui::BeginTable("##ncFields", 2,
					ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX))
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Checkbox(TRANSLATE("counter.tick").c_str(), &counter->tick.shown);
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox(TRANSLATE("counter.time").c_str(), &counter->timeSeconds.shown);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Checkbox(TRANSLATE("counter.bpm").c_str(), &counter->bpm.shown);
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox(TRANSLATE("counter.notes").c_str(), &counter->notesPassed.shown);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Checkbox(TRANSLATE("counter.nps").c_str(), &counter->notesPerSecond.shown);
					ImGui::TableSetColumnIndex(1);
					ImGui::Checkbox(TRANSLATE("counter.poly").c_str(), &counter->polyphony.shown);

					ImGui::EndTable();
				}

				break;
			}
			}
		});
}

void SettingsDialog::DrawAudioTab()
{
	MIDIAudio* midiAudio = app->GetMIDIAudio();
	Localization* localization = app->GetLocalization();

	{
		ForumLayout layout("##audioEngine");

		layout.AddField("##engine",
			TRANSLATE("dialog.settings.audio.engine").c_str(),
			[&]()
			{
				AudioEngineList& engineList = midiAudio->GetEngineList();
				if (ImGui::BeginCombo("", midiAudio->GetCurrentEngine()->GetName().c_str()))
				{
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
						if (ImGui::Selectable(engine->GetName().c_str(), midiAudio->GetCurrentEngineType() == type))
							midiAudio->SwitchEngine(type);

						if (!isSupported)
							ImGui::SetItemTooltip(TRANSLATE("dialog.settings.audio.engine.unsupported").c_str());
						ImGui::EndDisabled();

						ImGui::Spacing();

						engineIndex++;
					}
					ImGui::EndCombo();
				}
			});
	}
	
	if (ImGui::CollapsingHeader("Engine Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		midiAudio->GetCurrentEngine()->RenderSettings();
	}
}

void SettingsDialog::DrawMIDITab()
{
	auto* config = app->GetConfig();
	Localization* localization = app->GetLocalization();

	ForumLayout layout("##midiSettings");

	layout.AddField("##midiThreads",
		TRANSLATE("dialog.settings.midi.threads").c_str(),
		[&]()
		{
			if (ImGui::RadioButton(TRANSLATE("dialog.settings.midi.threads.0").c_str(), !config->midi.multithreadedLoading))
				config->midi.multithreadedLoading = false;

			if (ImGui::RadioButton(TRANSLATE("dialog.settings.midi.threads.1").c_str(), config->midi.multithreadedLoading))
				config->midi.multithreadedLoading = true;
		});

	layout.AddField("##timebasedLoading",
		TRANSLATE("dialog.settings.midi.time").c_str(),
		[&]()
		{
			ImGui::Checkbox("##timebasedLoading", &config->midi.timeBasedLoading);
		});
}