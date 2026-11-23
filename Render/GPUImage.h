#pragma once

#include <iostream>
#include <fstream>
#include <memory>
#include <vector>

typedef unsigned int GLuint;

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

	GLuint GetTextureID() { return texture; }
	void SetLinearFiltering();
private:
	GLuint texture = 0;
	GLuint slot = 0;
	bool validTexture = false;

	void DeleteTexture();
	void Bind(GLuint slot);
	void Unbind() const;
	void LoadMissingTexture();

	friend class TextureBind;
};

class TextureBind
{
public:
	TextureBind(GPUImage& image, GLuint slot);
	TextureBind(GLuint texture, GLuint slot);
	~TextureBind();
private:
	GLuint slot;
	GLuint texture;
};