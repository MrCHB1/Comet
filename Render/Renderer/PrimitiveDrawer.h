#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

typedef int GLsizei;

class ShaderProgram;
class VertexArray;
class Buffer;

#pragma pack(push, 1)
struct Vertex
{
	glm::vec2 position;
	glm::vec2 uv;
};
#pragma pack(pop)

struct PrimitiveTransform
{
	glm::vec3 position;
	glm::vec2 scale;
};

class Primitive
{
public:
	Primitive(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	~Primitive();

	void SetShader(std::shared_ptr<ShaderProgram> program);
	void SetTransform(const PrimitiveTransform& transform, bool resetTransform = true);
	void SetColor(const glm::vec3& color);
	void Draw() const;
private:
	std::shared_ptr<ShaderProgram> program;
	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<Buffer> vbo;
	std::unique_ptr<Buffer> ebo;

	glm::mat4 transform = glm::mat4(1.0f);

	GLsizei indexCount = 0;
};