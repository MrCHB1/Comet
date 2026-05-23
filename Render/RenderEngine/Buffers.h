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