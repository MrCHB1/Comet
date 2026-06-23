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

#pragma region Framebuffers

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
{
	fbo = other.fbo;
	other.fbo = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
	if (this != &other)
	{
		if (fbo != 0)
			glDeleteFramebuffers(1, &fbo);
		fbo = other.fbo;
		other.fbo = 0;
	}
	return *this;
}

void Framebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Framebuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Setup(int width, int height, GLint internalFormat, GLenum format, GLenum type)
{
	FramebufferBind bind(*this);
	glGenTextures(1, &sceneTexture);
	glBindTexture(GL_TEXTURE_2D, sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);

	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer is not complete!" << std::endl;
	}

	this->width = width;
	this->height = height;
	this->internalFormat = internalFormat;
	this->format = format;
	this->type = type;
}

void Framebuffer::Resize(int width, int height)
{
	glBindTexture(GL_TEXTURE_2D, sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	this->width = width;
	this->height = height;
}

#pragma endregion