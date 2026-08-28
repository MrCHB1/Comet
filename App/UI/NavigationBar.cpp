#include "NavigationBar.h"
#include "imgui.h"
#include <algorithm>
#include "Utils.h"
#include "App/MIDIApp.h"
#include "MIDI/Timer/MIDITimer.h"
#include "App/UI/Widgets/ThinSlider.h"

#define NAVIGATION_ROWS 2
#define MIN_MIDI_TIME_SECS -3.0f

#define UI_ATLAS_CELL_X 4
#define UI_ATLAS_CELL_Y 2

#define ATLAS_UV(atlas, x, y) \
    ImVec2((float)(x) / UI_ATLAS_CELL_X, (float)(y) / UI_ATLAS_CELL_Y), \
    ImVec2((float)(x + 1) / UI_ATLAS_CELL_X, (float)(y + 1) / UI_ATLAS_CELL_Y)

// TODO: Implement reset when unloading midi

NavigationBar::NavigationBar(MIDIApp* app, RenderView* renderView)
{
	this->app = app;
	this->renderView = renderView;
}

void NavigationBar::TryLoadUITextures()
{
	if (texturesLoaded) return;

#define LOAD_TEXTURE(target, path) \
	target = std::make_unique<GPUImage>(Utils::TryGetStream(path))

	LOAD_TEXTURE(uiTexture, "./assets/textures/ui.png");

#undef LOAD_TEXTURE

	texturesLoaded = true;
}

void NavigationBar::VerticalSeparator(float height, bool altColor)
{
	if (height <= 0.0f) height = ImGui::GetFrameHeight();
	ImVec2 pos = ImGui::GetCursorScreenPos();

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(pos.x, pos.y),
		ImVec2(pos.x, pos.y + height),
		ImGui::GetColorU32(altColor ? ImGuiCol_Text : ImGuiCol_Separator)
	);

	ImGui::Dummy(ImVec2(1.0f, height));
}

void NavigationBar::Draw()
{
	if (!app->IsGLReady()) return;
	TryLoadUITextures();

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 2));
	ImVec2 pos = viewport->WorkPos;
	ImGui::SetNextWindowPos(pos);
	lastPosition = glm::vec2(pos.x, pos.y);

	float frameHeight = ImGui::GetFrameHeight();
	float framePadding = ImGui::GetStyle().FramePadding.y;

	float spacing = ImGui::GetStyle().ItemSpacing.y;
	float toolbarHeight = frameHeight * NAVIGATION_ROWS + spacing * (NAVIGATION_ROWS - 1) + ImGui::GetStyle().WindowPadding.y * 2.0f;
	
	ImVec2 size = ImVec2(viewport->WorkSize.x, toolbarHeight);
	ImGui::SetNextWindowSize(size);
	lastResolution = glm::vec2(size.x, size.y);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	auto& style = ImGui::GetStyle();
	auto& c = style.Colors;
	ImVec4 textCol = c[ImGuiCol_Text];
	ImVec4 bgCol = c[ImGuiCol_WindowBg];
	ImVec4 frameCol = c[ImGuiCol_FrameBg];

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(bgCol.x, bgCol.y, bgCol.z, 0.8f));
	
	if (ImGui::Begin("MainDockWindow", nullptr, flags))
	{
		float imageSize = frameHeight - framePadding * 2.0f;
		MIDITimer* timer = app->GetTimer().get();
		// top bar
		{
			{
				if (ImGui::ImageButton("##playBtn",
					(ImTextureID)(intptr_t)uiTexture->GetRawTexture(),
					ImVec2(imageSize, imageSize),
					ATLAS_UV(uiTexture, 0, 0),
					ImVec4(0, 0, 0, 0),
					textCol))
				{
					timer->Resume();
				}
				ImGui::SameLine();
				if (ImGui::ImageButton("##pauseBtn",
					(ImTextureID)(intptr_t)uiTexture->GetRawTexture(),
					ImVec2(imageSize, imageSize),
					ATLAS_UV(uiTexture, 1, 0),
					ImVec4(0, 0, 0, 0),
					textCol))
				{
					timer->Pause();
				}
				ImGui::SameLine();
				if (ImGui::ImageButton("##stopBtn",
					(ImTextureID)(intptr_t)uiTexture->GetRawTexture(),
					ImVec2(imageSize, imageSize),
					ATLAS_UV(uiTexture, 2, 0),
					ImVec4(0, 0, 0, 0),
					textCol))
				{
					timer->Stop();
				}
			}

			ImGui::SameLine();
			VerticalSeparator(0.0f, true);
			ImGui::SameLine();

			{
				MIDIPlayerConfig* config = app->GetConfig();

				if (ImGui::ImageButton("##backBtn",
					(ImTextureID)(intptr_t)uiTexture->GetRawTexture(),
					ImVec2(imageSize, imageSize),
					ATLAS_UV(uiTexture, 0, 1),
					ImVec4(0, 0, 0, 0),
					textCol))
				{
					float jump = config->navigation.seekBackwardSeconds;
					float targetTime = timer->Elapsed() - jump;
					if (targetTime < MIN_MIDI_TIME_SECS) targetTime = MIN_MIDI_TIME_SECS;

					timer->NavigateTo(targetTime);
				}

				ImGui::SameLine();

				if (ImGui::ImageButton("##forwardBtn",
					(ImTextureID)(intptr_t)uiTexture->GetRawTexture(),
					ImVec2(imageSize, imageSize),
					ATLAS_UV(uiTexture, 3, 0),
					ImVec4(0, 0, 0, 0),
					textCol))
				{
					float jump = config->navigation.seekForwardSeconds;
					float targetTime = timer->Elapsed() + jump;
					if (targetTime > midiLength) targetTime = midiLength;

					timer->NavigateTo(targetTime);
				}
			}

			ImGui::SameLine();
			VerticalSeparator(0.0f, true);
			ImGui::SameLine();

			ImGui::SetNextItemWidth(170);
			int viewTicks = static_cast<int>(renderView->viewTicks);

			if (ThinSlider::Draw<int>("##navNoteSize", &viewTicks, 48, 7680, "", ImGuiSliderFlags_Logarithmic))
			{
				renderView->viewTicks = std::clamp(viewTicks, 48, 7680);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Note size (in ticks)");
			}
		}

		// time bar
		{
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			float currTimeSecs = timer->Elapsed();

			if (ThinSlider::Draw<float>("##navMIDITime", &currTimeSecs, MIN_MIDI_TIME_SECS, midiLength, "", ImGuiSliderFlags_NoInput))
			{
				if (currTimeSecs != timer->Elapsed())
				{
					timer->NavigateTo(currTimeSecs);
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	Update();
}

void NavigationBar::Update()
{

}

#undef NAVIGATION_ROWS
#undef MIN_MIDI_TIME_SECS