#pragma once
#include "Renderer/PrimitiveShaders.h"
#include <memory>
#include "Renderer/QuadDrawer.h"
#include "GPUImage.h"

// draws a quad at a given transform that samples the scene texture and applies a blur to it, for use in things like the note counter background
class BlurredQuadRenderer
{
public:
	BlurredQuadRenderer()
	{
		blurredQuad = std::make_unique<Quad>();
		blurredQuad->SetShader(BLUR_SHADER);
	}

	void SetSceneTexture(GLuint sceneTexture)
	{
		ShaderBind bind(*BLUR_SHADER);
		BLUR_SHADER->SetInt("scene", 0);
		this->sceneTexture = sceneTexture;
	}

	void Render(const PrimitiveTransform& transform)
	{
		if (sceneTexture == 0)
		{
			std::cout << "Attempt to render blurred quad without a valid scene texture." << std::endl;
			return;
		}

		TextureBind textureBind(sceneTexture, 0);
		blurredQuad->SetTransform(transform);
		blurredQuad->Draw();
	}
private:
	std::unique_ptr<Quad> blurredQuad;
	GLuint sceneTexture = 0;
};