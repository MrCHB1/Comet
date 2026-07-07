#include "GPUImage.h"
#include <vector>

bool GPUImage::LoadFromStream(std::shared_ptr<std::istream> file)
{
	if (!file)
	{
		std::cout << "File doesn't exist" << std::endl;
		return false;
	}

	std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(*file.get())), std::istreambuf_iterator<char>());
	int channels;

	unsigned char* pixels = stbi_load_from_memory(buffer.data(), buffer.size(), &width, &height, &channels, 4);
	if (!pixels)
	{
		std::cout << "Failed: " << stbi_failure_reason() << std::endl;
		return false;
	}

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(pixels);
	pixels = nullptr;

	return true;
}

GPUImage::GPUImage(std::vector<unsigned char> data, int width, int height)
{
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
		data.data()
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
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

void GPUImage::Bind(GLuint slot)
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texture);
	this->slot = slot;
}

void GPUImage::Unbind() const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, 0);
}