#include "Buffers.h"

#pragma region Buffers

Buffer::Buffer(Buffer&& other) noexcept
{
	buffer = other.buffer;
	target = other.target;
	other.buffer = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
	if (this != &other)
	{
		if (buffer != 0)
			glDeleteBuffers(1, &buffer);

		buffer = other.buffer;
		target = other.target;
		other.buffer = 0;
	}
	return *this;
}

void Buffer::Bind() const
{
	glBindBuffer(target, buffer);
}

void Buffer::Unbind() const
{
	glBindBuffer(target, 0);
}
#pragma endregion

#pragma region Vertex Arrays

VertexArray::VertexArray(VertexArray&& other) noexcept
{
	vao = other.vao;
	other.vao = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept
{
	if (this != &other)
	{
		if (vao != 0) glDeleteVertexArrays(1, &vao);

		vao = other.vao;
		other.vao = 0;
	}
	return *this;
}

void VertexArray::Bind() const
{
	glBindVertexArray(vao);
}

void VertexArray::Unbind() const
{
	glBindVertexArray(0);
}

void VertexArray::SetFloatAttribute(GLuint index, GLint components, GLsizei stride, size_t offset)
{
	glEnableVertexAttribArray(index);
	glVertexAttribPointer(
		index,
		components,
		GL_FLOAT,
		GL_FALSE,
		stride,
		(void*)offset
	);
}

void VertexArray::SetIntAttribute(GLuint index, GLint components, GLsizei stride, size_t offset)
{
	glEnableVertexAttribArray(index);
	glVertexAttribIPointer(
		index,
		components,
		GL_UNSIGNED_INT,
		stride,
		(void*)offset
	);
}

#pragma endregion