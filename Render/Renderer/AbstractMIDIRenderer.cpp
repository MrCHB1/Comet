#include "AbstractMIDIRenderer.h"
#include "App/MIDIApp.h"
#include "PrimitiveShaders.h"
#include "App/Dialog/DialogMacros.h"

void AbstractMIDIRenderer::Initialize()
{
	MIDIPlayerConfig* cfg = app->GetConfig();
	int width = cfg->render.GetWidth();
	int height = cfg->render.GetHeight();

	#pragma region Framebuffer creation

	sceneFramebuffer = std::make_unique<Framebuffer>();
	sceneFramebuffer->Setup(width, height);
	fullscreenQuad = std::make_unique<Quad>();
	fullscreenQuad->SetShader(SCENE_SHADER);
	BLUR_SHADER->SetFloat("width", (float)width);
	BLUR_SHADER->SetFloat("height", (float)height);
	fullscreenQuad->SetTransform({ glm::vec3(0.0f), glm::vec2(1.0f) }, false);

	#pragma endregion
}

void AbstractMIDIRenderer::RenderSettings()
{
	MIDIPlayerConfig* config = app->GetConfig();
	ImGui::Separator();
	BEGIN_SECTION("##globalSettings")
	{
		SETUP_SECTION;

		SECTION_ENTRY(SECTION_LABEL("Background color"),
			{
				ImVec4 bgColorVec = config->render.GetBackground();
				float bgColor[3]{ bgColorVec.x, bgColorVec.y, bgColorVec.z };
				if (ImGui::ColorEdit3("##bgColor", bgColor))
				{
					config->render.SetBackground(bgColor[0], bgColor[1], bgColor[2]);
				}
			});

		SECTION_ENTRY(SECTION_LABEL("Key range"),
			{
				int keyFirst = config->render.GetKeyFirst();
				int keyLast = config->render.GetKeyLast();
				if (ImGui::InputInt("##keyFirst", &keyFirst))
				{
					config->render.SetKeyFirst(keyFirst);
				}
				ImGui::SameLine();
				if (ImGui::InputInt("##keyLast", &keyLast))
				{
					config->render.SetKeyLast(keyLast);
				}
			});

		END_SECTION;
	}
	
	if (ImGui::Button("Reset settings")) ResetSettings();
}