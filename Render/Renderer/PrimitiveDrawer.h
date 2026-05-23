#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "../RenderEngine/Buffers.h"
#include "../RenderEngine/Shaders.h"

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
	void SetTransform(const PrimitiveTransform& transform, bool resetTransform = true);
	void SetColor(const glm::vec3& color);
	void Draw() const;
private:
	std::unique_ptr<ShaderProgram> program;
	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<Buffer> vbo;
	std::unique_ptr<Buffer> ebo;

	glm::mat4 transform = glm::mat4(1.0f);

	GLsizei indexCount = 0;
};