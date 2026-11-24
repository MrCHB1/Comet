#pragma once
#include <memory>
#include <istream>
#include <glm/glm.hpp>

typedef unsigned int GLuint;
typedef unsigned int GLenum;

class TextureArrayBind;

class TextureArray
{
public:
	TextureArray(int width, int height, int layerCount, GLenum internalFormat = 0x8058);
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

	[[nodiscard]] GLuint GetRawTexture() const { return texture; }
	[[nodiscard]] int GetWidth() const { return width; }
	[[nodiscard]] int GetHeight() const { return height; }
	[[nodiscard]] int GetLayerCount() const { return layerCount; }
	[[nodiscard]] float GetLayerAspectRatio(int layer) const
	{
		if (layer < 0 || layer >= layerCount) return 1.0f;
		return layerAspectRatios[layer];
	}
	
	// Peeks an image's pixel dimensions without fully decoding it, and rewinds
	// the stream afterwards. Use this up front to size the array correctly
	// (e.g. take the max width/height across every image you plan to load).
	static bool ProbeDimensions(std::shared_ptr<std::istream> file, int& outWidth, int& outHeight);

	void SetLinearFiltering();

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
	TextureArrayBind(TextureArray& array, GLuint slot);
	~TextureArrayBind();
private:
	TextureArray& array;
	GLuint slot;
};