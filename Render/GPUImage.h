#pragma once
#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>


class TextureBind;

class GPUImage
{
public:
	int width = 0, height = 0;
	GPUImage(std::ifstream& file)
	{
		if (!file) { std::cout << "Image stream does not exist" << std::endl; return; }
		if (!LoadFromStream(file))
		{
			std::cout << "Failed to load image" << std::endl;
		}
	}
	~GPUImage()
	{
		DeleteTexture();
	}
	GPUImage(const GPUImage&) = delete;
	GPUImage& operator=(const GPUImage&) = delete;

	GPUImage(GPUImage&& other) noexcept;
	GPUImage& operator=(GPUImage&& other) noexcept;

	bool LoadFromStream(std::ifstream& file);
	GLuint GetRawTexture() const { return texture; }
private:
	GLuint texture = 0;

	void DeleteTexture();
	void Bind(GLuint slot) const;
	void Unbind() const;

	friend class TextureBind;
};

class TextureBind
{
public:
	TextureBind(GPUImage& image, GLuint slot) : texture(image.GetRawTexture())
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	TextureBind(GLuint texture, GLuint slot) : texture(texture)
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	~TextureBind() { glBindTexture(GL_TEXTURE_2D, 0); }
private:
	GLuint texture;
};