#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shaders.h"
#include "Comet.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <optional>

static std::optional<std::string> ReadFile(const std::string& path)
{
	std::ifstream fileStream(path.c_str(), std::ios::in | std::ios::binary);
	if (!fileStream)
	{
		std::cout << "Failed to open file: " << path << std::endl;
		return std::nullopt;
	}

	std::ostringstream fileString;
	fileString << fileStream.rdbuf();
	fileStream.close();

	return fileString.str();
}

static bool CheckShaderCompile(GLuint shader)
{
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success) return true;

	GLint logSize = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);

	if (logSize <= 1) return false;

	std::string log(logSize, '\0');
	glGetShaderInfoLog(shader, logSize, nullptr, log.data());

	std::cout << "======== SHADER COMPILATION ERROR ========" << std::endl;
	std::cout << log << std::endl;
	return false;
}

static bool CheckShaderLinkStatus(GLuint program)
{
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success) return true;

	GLint logSize = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);

	if (logSize <= 1) return false;

	std::string log(logSize, '\0');
	glGetProgramInfoLog(program, logSize, nullptr, log.data());

	std::cout << "======== SHADER LINK ERROR ========" << std::endl;
	std::cout << log << std::endl;
	return false;
}

std::unique_ptr<ShaderProgram> ShaderProgram::CreateFromFiles(const std::string& path)
{
	std::optional<std::string> vertSource = ReadFile(path + ".vert");
	if (!vertSource.has_value())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to open file. Returning nullptr" << std::endl;
#endif
		return nullptr;
	}

	std::optional<std::string> fragSource = ReadFile(path + ".frag");
	if (!fragSource.has_value())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to open file. Returning nullptr" << std::endl;
#endif
		return nullptr;
	}

	return Create(vertSource->c_str(), fragSource->c_str());
}

std::unique_ptr<ShaderProgram> ShaderProgram::CreateFromFiles(const std::string& vertPath, const std::string& fragPath)
{
	std::optional<std::string> vertSource = ReadFile(vertPath + ".vert");
	if (!vertSource.has_value())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to open file. Returning nullptr" << std::endl;
#endif
		return nullptr;
	}

	std::optional<std::string> fragSource = ReadFile(fragPath + ".frag");
	if (!fragSource.has_value())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to open file. Returning nullptr" << std::endl;
#endif
		return nullptr;
	}

	return Create(vertSource->c_str(), fragSource->c_str());
}

std::unique_ptr<ShaderProgram> ShaderProgram::Create(const char* vertSrc, const char* fragSrc)
{
	std::cout << "---- Compiling Vertex Shader ----" << std::endl;

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertSrc, nullptr);
	glCompileShader(vertexShader);
	if (!CheckShaderCompile(vertexShader))
	{
		glDeleteShader(vertexShader);
		return nullptr;
	}
	std::cout << "Success!" << std::endl;

	std::cout << "---- Compiling Fragment Shader ----" << std::endl;
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragSrc, nullptr);
	glCompileShader(fragmentShader);
	if (!CheckShaderCompile(fragmentShader))
	{
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return nullptr;
	}

	std::cout << "Success!" << std::endl;
	std::cout << "---- Linking Shaders ----" << std::endl;

	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (!CheckShaderLinkStatus(program))
	{
		return nullptr;
	}

	std::cout << "Success!" << std::endl;
	return std::make_unique<ShaderProgram>(program);
}

std::unique_ptr<ShaderProgram> ShaderProgram::CreateComputeFromFile(const std::string& path)
{
	std::optional<std::string> computeSource = ReadFile(path + ".comp");
	if (!computeSource.has_value())
	{
#ifdef COMET_DEBUG
		std::cout << "Failed to open file. Returning nullptr" << std::endl;
#endif
		return nullptr;
	}

	return CreateCompute(computeSource->c_str());
}

std::unique_ptr<ShaderProgram> ShaderProgram::CreateCompute(const char* computeSrc)
{
	std::cout << "---- Compiling Compute Shader ----" << std::endl;

	GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(computeShader, 1, &computeSrc, nullptr);
	glCompileShader(computeShader);
	if (!CheckShaderCompile(computeShader))
	{
		glDeleteShader(computeShader);
		return nullptr;
	}
	std::cout << "Success!" << std::endl;

	std::cout << "---- Linking Compute Shader ----" << std::endl;
	GLuint program = glCreateProgram();
	glAttachShader(program, computeShader);
	glLinkProgram(program);
	glDeleteShader(computeShader);

	if (!CheckShaderLinkStatus(program))
	{
		return nullptr;
	}

	std::cout << "Success!" << std::endl;
	return std::make_unique<ShaderProgram>(program);
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
{
	program = other.program;
	other.program = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
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

ShaderProgram::~ShaderProgram()
{
	// cleanup
	if (!IsValidProgram()) return;
	glDeleteProgram(program);
}

void ShaderProgram::SetInt(const char* uniform, int value)
{
	if (!IsValidProgram()) return;
	glUniform1i(glGetUniformLocation(program, uniform), value);
}

void ShaderProgram::SetUInt(const char* uniform, unsigned int value)
{
	if (!IsValidProgram()) return;
	glUniform1ui(glGetUniformLocation(program, uniform), value);
}

void ShaderProgram::SetFloat(const char* uniform, float value)
{
	if (!IsValidProgram()) return;
	glUniform1f(glGetUniformLocation(program, uniform), value);
}

void ShaderProgram::SetMat4(const char* uniform, const glm::mat4& matrix)
{
	if (!IsValidProgram()) return;
	glUniformMatrix4fv(glGetUniformLocation(program, uniform), 1, GL_FALSE, glm::value_ptr(matrix));
}

void ShaderProgram::SetVec2(const char* uniform, const glm::vec2& vector)
{
	if (!IsValidProgram()) return;
	glUniform2f(glGetUniformLocation(program, uniform), vector.x, vector.y);
}

void ShaderProgram::SetVec3(const char* uniform, const glm::vec3& vector)
{
	if (!IsValidProgram()) return;
	glUniform3f(glGetUniformLocation(program, uniform), vector.x, vector.y, vector.z);
}

void ShaderProgram::Bind()
{
	if (!IsValidProgram()) return;
	glUseProgram(this->program);
}

void ShaderProgram::Unbind()
{
	glUseProgram(0);
}

bool ShaderProgram::IsValidProgram() noexcept
{
	return program != 0 && glIsProgram(program);
}

ShaderBind::ShaderBind(ShaderProgram& s) { s.Bind(); }
ShaderBind::~ShaderBind() { glUseProgram(0); }