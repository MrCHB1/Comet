#include "Shaders.h"
#include "Comet.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <optional>

static std::optional<std::string> ReadFile(const std::string & path)
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

void ShaderProgram::Bind()
{
	if (!IsValidProgram()) return;
	glUseProgram(this->program);
}

void ShaderProgram::Unbind()
{
	glUseProgram(0);
}