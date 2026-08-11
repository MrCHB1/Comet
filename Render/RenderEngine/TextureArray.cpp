#include "TextureArray.h"
#include <iostream>
#include <vector>
#include <algorithm>

TextureArray::TextureArray(int width, int height, int layerCount, GLenum internalFormat)
	: width(width), height(height), layerCount(layerCount), internalFormat(internalFormat), layerAspectRatios(layerCount, 1.0f)
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

	// Allocate storage for every layer up front. glTexImage3D (rather than
	// glTexStorage3D) is used since it works down to GL 3.0/3.3 core, matching
	// the rest of this renderer.
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, width, height, layerCount,
		0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

TextureArray::~TextureArray()
{
	if (texture != 0 && glIsTexture(texture))
		glDeleteTextures(1, &texture);
}

TextureArray::TextureArray(TextureArray&& other) noexcept
{
	texture = other.texture;
	slot = other.slot;
	width = other.width;
	height = other.height;
	layerCount = other.layerCount;
	internalFormat = other.internalFormat;
	layerAspectRatios = std::move(other.layerAspectRatios);
	other.texture = 0;
}

TextureArray& TextureArray::operator=(TextureArray&& other) noexcept
{
	if (this != &other)
	{
		if (texture != 0 && glIsTexture(texture))
			glDeleteTextures(1, &texture);

		texture = other.texture;
		slot = other.slot;
		width = other.width;
		height = other.height;
		layerCount = other.layerCount;
		internalFormat = other.internalFormat;
		layerAspectRatios = std::move(other.layerAspectRatios);
		other.texture = 0;
	}
	return *this;
}

bool TextureArray::ProbeDimensions(std::shared_ptr<std::istream> file, int& outWidth, int& outHeight)
{
	if (!file) return false;

	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(*file)), std::istreambuf_iterator<char>());
	int channels;
	if (!stbi_info_from_memory(buffer.data(), (int)buffer.size(), &outWidth, &outHeight, &channels))
		return false;

	// Rewind so a later LoadLayer() call can still read the same stream.
	file->clear();
	file->seekg(0);
	return true;
}

glm::vec4 TextureArray::LoadLayer(int layer, std::shared_ptr<std::istream> file)
{
	if (!file || layer < 0 || layer >= layerCount)
	{
		std::cout << "TextureArray: invalid layer index or missing file stream" << std::endl;
		return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	}

	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(*file)), std::istreambuf_iterator<char>());
	int imgWidth, imgHeight, channels;

	stbi_set_flip_vertically_on_load(true);
	unsigned char* pixels = stbi_load_from_memory(buffer.data(), (int)buffer.size(), &imgWidth, &imgHeight, &channels, 4);
	if (!pixels)
	{
		std::cout << "TextureArray: failed to decode layer " << layer << ": " << stbi_failure_reason() << std::endl;
		return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	}

	if (imgWidth > width || imgHeight > height)
	{
		std::cout << "TextureArray: layer " << layer << " image (" << imgWidth << "x" << imgHeight
			<< ") is larger than the array canvas (" << width << "x" << height
			<< ") - it will be cropped. Re-size the array up front instead." << std::endl;
	}

	if (imgHeight > 0)
	{
		layerAspectRatios[layer] = static_cast<float>(imgWidth) / static_cast<float>(imgHeight);
	}

	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexSubImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,
		0, 0, layer,
		std::min(imgWidth, width), std::min(imgHeight, height), 1,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		pixels
	);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	stbi_image_free(pixels);
	pixels = nullptr;

	return glm::vec4(
		0.0f,
		0.0f,
		(float)imgWidth / (float)width,
		(float)imgHeight / (float)height
	);
}

void TextureArray::Bind(GLuint slot)
{
	this->slot = slot;
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
}

void TextureArray::Unbind() const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}