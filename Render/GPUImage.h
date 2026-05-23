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
	TextureBind(GPUImage& image, GLuint slot) : image(image) { image.Bind(slot); }
	~TextureBind() { image.Unbind(); }
private:
	GPUImage& image;
};