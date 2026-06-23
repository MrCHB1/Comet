#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class ShaderProgram
{
public:
	static std::unique_ptr<ShaderProgram> CreateFromFiles(const std::string& filePath);
	static std::unique_ptr<ShaderProgram> CreateFromFiles(const std::string& vertPath, const std::string& fragPath);
	static std::unique_ptr<ShaderProgram> Create(const char* vertSrc, const char* fragSrc);
	ShaderProgram(GLuint program) : program(program) {}

	// only allow shader programs to move around
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	ShaderProgram(ShaderProgram&& other) noexcept
	{
		program = other.program;
		other.program = 0;
	}

	ShaderProgram& operator=(ShaderProgram&& other) noexcept
	{
		if (this != &other)
		{
			if (IsValidProgram())
				glDeleteProgram(program);

			program = other.program;
			other.program = 0;
		}
		return *this;
	}

	~ShaderProgram()
	{
		// cleanup
		if (IsValidProgram())
			glDeleteProgram(program);
	}

	void SetInt(const char* uniform, int value)
	{
		if (!IsValidProgram()) return;
		glUniform1i(glGetUniformLocation(program, uniform), value);
	}

	void SetFloat(const char* uniform, float value)
	{
		if (!IsValidProgram()) return;
		glUniform1f(glGetUniformLocation(program, uniform), value);
	}

	void SetMat4(const char* uniform, const glm::mat4& matrix)
	{
		if (!IsValidProgram()) return;
		glUniformMatrix4fv(glGetUniformLocation(program, uniform), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void SetVec2(const char* uniform, const glm::vec2& vector)
	{
		if (!IsValidProgram()) return;
		glUniform2f(glGetUniformLocation(program, uniform), vector.x, vector.y);
	}

	void SetVec3(const char* uniform, const glm::vec3& vector)
	{
		if (!IsValidProgram()) return;
		glUniform3f(glGetUniformLocation(program, uniform), vector.x, vector.y, vector.z);
	}

	GLuint GetID()
	{
		return program;
	}

private:
	GLuint program;
	bool IsValidProgram()
	{
		return program != 0 && glIsProgram(program);
	}

	// bind/unbind defined here to enforce RAII.
	void Bind();
	void Unbind();

	friend class ShaderBind;
};

// helper class for binding shaders and automatically unbinding them
class ShaderBind
{
public:
	ShaderBind(ShaderProgram& s) { s.Bind(); }
	~ShaderBind() { glUseProgram(0); }
};