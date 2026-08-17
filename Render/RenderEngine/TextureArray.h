#pragma once
#include "../../stb_image.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <memory>
#include <istream>
#include <glm/glm.hpp>

class TextureArrayBind;

class TextureArray
{
public:
	TextureArray(int width, int height, int layerCount, GLenum internalFormat = GL_RGBA8);
	~TextureArray();

	TextureArray(const TextureArray&) = delete;
	TextureArray& operator=(const TextureArray&) = delete;

	TextureArray(TextureArray&& other) noexcept;
	TextureArray& operator=(TextureArray&& other) noexcept;

	// Decodes the image and uploads it into the given layer (0-based).
	// Returns the UV rect mapping to the real image content within the canvas.
	// Returns (0,0,1,1) and logs on failure so it's always safe to use.
	glm::vec4 LoadLayer(int layer, std::shared_ptr<std::istream> file);

	void Bind(GLuint slot);
	void Unbind() const;

	GLuint GetRawTexture() const { return texture; }
	int GetWidth() const { return width; }
	int GetHeight() const { return height; }
	int GetLayerCount() const { return layerCount; }
	float GetLayerAspectRatio(int layer) const
	{
		if (layer < 0 || layer >= layerCount) return 1.0f;
		return layerAspectRatios[layer];
	}
	
	// Peeks an image's pixel dimensions without fully decoding it, and rewinds
	// the stream afterwards. Use this up front to size the array correctly
	// (e.g. take the max width/height across every image you plan to load).
	static bool ProbeDimensions(std::shared_ptr<std::istream> file, int& outWidth, int& outHeight);

private:
	GLuint texture = 0;
	GLuint slot = 0;
	int width = 0, height = 0, layerCount = 0;
	GLenum internalFormat;
	std::vector<float> layerAspectRatios;

	friend class TextureArrayBind;
};

class TextureArrayBind
{
public:
	TextureArrayBind(TextureArray& array, GLuint slot) : array(array), slot(slot)
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D_ARRAY, array.texture);
	}
	~TextureArrayBind()
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}
private:
	TextureArray& array;
	GLuint slot;
};