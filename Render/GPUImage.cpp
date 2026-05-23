#include "GPUImage.h"
#include <vector>

bool GPUImage::LoadFromStream(std::ifstream& file)
{
	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	int channels;

	unsigned char* pixels = stbi_load_from_memory(buffer.data(), buffer.size(), &width, &height, &channels, 4);
	if (!pixels) return false;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		width,
		height,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		pixels
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	stbi_image_free(pixels);
	pixels = nullptr;

	return true;
}

GPUImage::GPUImage(GPUImage&& other) noexcept
{
	texture = other.texture;
	width = other.width;
	height = other.height;
	other.texture = 0;
}

GPUImage& GPUImage::operator=(GPUImage&& other) noexcept
{
	if (this != &other)
	{
		DeleteTexture();

		texture = other.texture;
		width = other.width;
		height = other.height;
		other.texture = 0;
	}

	return *this;
}

void GPUImage::DeleteTexture()
{
	if (texture != 0 && glIsTexture(texture))
		glDeleteTextures(1, &texture);
}

void GPUImage::Bind(GLuint slot) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texture);
}

void GPUImage::Unbind() const
{
	glBindTexture(GL_TEXTURE_2D, 0);
}