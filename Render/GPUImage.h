#pragma once
#include "stb_image.h"
#include "stb_image_write.h"
#include <iostream>
#include <fstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

inline std::vector<unsigned char> MISSING_TEXTURE_DATA = {
	0, 0, 0, 255,
	255, 0, 255, 255,
	255, 0, 255, 255,
	0, 0, 0, 255
};

class TextureBind;

class GPUImage
{
public:
	int width = 0, height = 0;
	GPUImage(std::shared_ptr<std::istream> file)
	{
		if (!file)
		{
			std::cout << "Image stream does not exist" << std::endl;
			LoadMissingTexture();
			return;
		}
		
		if (!LoadFromStream(file))
		{
			std::cout << "Failed to load image" << std::endl;
			LoadMissingTexture();
			return;
		}
		
		validTexture = true;
	}
	GPUImage(std::vector<unsigned char> data, int width, int height);
	~GPUImage()
	{
		DeleteTexture();
	}
	GPUImage(const GPUImage&) = delete;
	GPUImage& operator=(const GPUImage&) = delete;

	GPUImage(GPUImage&& other) noexcept;
	GPUImage& operator=(GPUImage&& other) noexcept;

	bool LoadFromStream(std::shared_ptr<std::istream> file);
	GLuint GetRawTexture() const { return texture; }
	bool IsValidTexture() const { return validTexture; }
private:
	GLuint texture = 0;
	GLuint slot = 0;
	bool validTexture = false;

	void DeleteTexture();
	void Bind(GLuint slot);
	void Unbind() const;
	void LoadMissingTexture()
	{
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			2,
			2,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			MISSING_TEXTURE_DATA.data()
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
		this->width = 2;
		this->height = 2;
	}

	friend class TextureBind;
};

class TextureBind
{
public:
	TextureBind(GPUImage& image, GLuint slot) : slot(slot), texture(image.GetRawTexture())
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	TextureBind(GLuint texture, GLuint slot) : slot(slot), texture(texture)
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, texture);
	}

	~TextureBind()
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
private:
	GLuint slot;
	GLuint texture;
};