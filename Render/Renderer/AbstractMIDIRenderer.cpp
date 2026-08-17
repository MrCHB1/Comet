#include "AbstractMIDIRenderer.h"
#include "../../App/MIDIApp.h"
#include "PrimitiveShaders.h"
#include "../../App/Dialog/DialogMacros.h"

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
				ImGui::SetNextItemWidth(100);
				if (ImGui::InputInt("##keyFirst", &keyFirst, 0, 0))
				{
					config->render.SetKeyFirst(keyFirst);
				}
				ImGui::SameLine();
				ImGui::Text("~");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100);
				if (ImGui::InputInt("##keyLast", &keyLast, 0, 0))
				{
					config->render.SetKeyLast(keyLast);
				}

				std::string previewText;
				
				if (keyFirst == 21 && keyLast == 108)
				{
					previewText = "88 Keys";
				}
				else if (keyFirst == 0 && keyLast == 127)
				{
					previewText = "128 Keys";
				}
				else
				{
					previewText = "Custom";
				}

				if (ImGui::BeginCombo("##keyRangePreset", previewText.c_str()))
				{
					if (ImGui::Selectable("88 Keys", keyFirst == 21 && keyLast == 108))
					{
						config->render.SetKeyFirst(21);
						config->render.SetKeyLast(108);
					}

					if (ImGui::Selectable("128 Keys", keyFirst == 0 && keyLast == 127))
					{
						config->render.SetKeyFirst(0);
						config->render.SetKeyLast(127);
					}

					ImGui::EndCombo();
				}
			});

		END_SECTION;
	}
	
	if (ImGui::Button("Reset settings")) ResetSettings();
}