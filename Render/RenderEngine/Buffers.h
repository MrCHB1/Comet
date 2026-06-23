#pragma once
#include "Comet.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

#pragma region Buffer

class BufferBind;

class Buffer
{
public:
	Buffer(GLenum target) : target(target)
	{
		glGenBuffers(1, &buffer);
	}

	~Buffer()
	{
		if (buffer != 0 && glIsBuffer(buffer)) glDeleteBuffers(1, &buffer);
	}

	// no copy, but move
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;

	// kinda buns that i have to define these in here rather than put them in their .cpp file.
	template <typename T>
	void SetData(const std::vector<T>& data, GLenum usage)
	{
		Bind();

		glBufferData(
			target,
			data.size() * sizeof(T),
			data.data(),
			usage
		);
	}

	template <typename T, std::size_t N>
	void SetData(const std::array<T, N>& data, GLenum usage)
	{
		Bind();

		glBufferData(
			target,
			N * sizeof(T),
			data.data(),
			usage
		);
	}

	template <typename T>
	void SetSubData(size_t offsetBytes, const std::vector<T>& data)
	{
		Bind();

		glBufferSubData(
			target,
			offsetBytes,
			data.size() * sizeof(T),
			data.data()
		);
	}

	void Bind() const;
	void Unbind() const;

private:
	GLuint buffer = 0;
	GLenum target;

	friend class BufferBind;
};

class BufferBind
{
public:
	BufferBind(Buffer& buffer) : buffer(buffer) { buffer.Bind(); }
	~BufferBind() { buffer.Unbind(); }
private:
	Buffer& buffer;
};

#pragma endregion

#pragma region Vertex Array

class VertexArray
{
public:
	VertexArray()
	{
		glGenVertexArrays(1, &vao);
	}
	~VertexArray()
	{
		glDeleteVertexArrays(1, &vao);
	}

	// no copy, allow move
	VertexArray(const VertexArray&) = delete;
	VertexArray& operator=(const VertexArray&) = delete;

	VertexArray(VertexArray&& other) noexcept;
	VertexArray& operator=(VertexArray&& other) noexcept;

	void SetFloatAttribute(GLuint index, GLint components, GLsizei stride, size_t offset);
	void SetIntAttribute(GLuint index, GLint components, GLsizei stride, size_t offset);

	GLuint GetID() { return vao; }

	void Bind() const;
	void Unbind() const;
private:
	GLuint vao;
	friend class VertexArrayBind;
};

class VertexArrayBind
{
public:
	VertexArrayBind(VertexArray& array) : array(array) { array.Bind(); }
	~VertexArrayBind() { array.Unbind(); }
private:
	VertexArray& array;
};

#pragma endregion

#pragma region Framebuffers

class Framebuffer
{
public:
	Framebuffer()
	{
		glGenFramebuffers(1, &fbo);
	}
	~Framebuffer()
	{
		if (fbo != 0 && glIsFramebuffer(fbo))
		{
			glDeleteFramebuffers(1, &fbo);
		}

		if (depthBuffer != 0)
		{
			glDeleteRenderbuffers(1, &depthBuffer);
			depthBuffer = 0;
		}
	}

	// no copy, allow move
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;

	Framebuffer(Framebuffer&& other) noexcept;
	Framebuffer& operator=(Framebuffer&& other) noexcept;

	// generates the textures
	void Setup(int width, int height, GLint internalFormat = GL_RGBA, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);
	// resizes framebuffer
	void Resize(int width, int height);

	int GetWidth() const { return width; }
	int GetHeight() const { return height; }

	GLuint GetSceneTexture() const { return sceneTexture; }

	void Bind() const;
	void Unbind() const;
	friend class FramebufferBind;
private:
	GLuint fbo;
	GLuint sceneTexture;
	GLuint depthBuffer;
	GLint internalFormat;
	GLenum format;
	GLenum type;
	int width = 0;
	int height = 0;
};

class FramebufferBind
{
public:
	FramebufferBind(Framebuffer& framebuffer) : framebuffer(framebuffer) { framebuffer.Bind(); }
	~FramebufferBind() { framebuffer.Unbind(); }
private:
	Framebuffer& framebuffer;
};

#pragma endregion