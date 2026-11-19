#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

typedef unsigned int GLuint;

class ShaderProgram
{
public:
	static std::unique_ptr<ShaderProgram> CreateFromFiles(const std::string& filePath);
	static std::unique_ptr<ShaderProgram> CreateFromFiles(const std::string& vertPath, const std::string& fragPath);
	static std::unique_ptr<ShaderProgram> Create(const char* vertSrc, const char* fragSrc);
	static std::unique_ptr<ShaderProgram> CreateComputeFromFile(const std::string& filePath);
	static std::unique_ptr<ShaderProgram> CreateCompute(const char* computeSrc);
	
	ShaderProgram(GLuint program) : program(program) {}

	// only allow shader programs to move around
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	ShaderProgram(ShaderProgram&& other) noexcept;
	ShaderProgram& operator=(ShaderProgram&& other) noexcept;

	~ShaderProgram();

	void SetInt(const char* uniform, int value);
	void SetUInt(const char* uniform, unsigned int value);
	void SetFloat(const char* uniform, float value);
	void SetMat4(const char* uniform, const glm::mat4& matrix);
	void SetVec2(const char* uniform, const glm::vec2& vector);
	void SetVec3(const char* uniform, const glm::vec3& vector);

	GLuint GetID()
	{
		return program;
	}

	void Bind();
	void Unbind();

private:
	GLuint program;
	[[nodiscard]] bool IsValidProgram() noexcept;

	friend class ShaderBind;
};

// helper class for binding shaders and automatically unbinding them
class ShaderBind
{
public:
	ShaderBind(ShaderProgram& s);
	~ShaderBind();
};