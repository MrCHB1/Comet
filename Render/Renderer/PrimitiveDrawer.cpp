#include "PrimitiveDrawer.h"
#include "PrimitiveShaders.h"

Primitive::Primitive(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	static_assert(sizeof(Vertex) == 16);

	indexCount = (GLsizei)indices.size();

	vao = std::make_unique<VertexArray>();
	vbo = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	ebo = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*vao);

		vbo->Bind();
		vbo->SetData(vertices, GL_STATIC_DRAW);

		ebo->Bind();
		ebo->SetData(indices, GL_STATIC_DRAW);

		vao->SetFloatAttribute(0, 2, sizeof(Vertex), offsetof(Vertex, position));
		vao->SetFloatAttribute(1, 2, sizeof(Vertex), offsetof(Vertex, uv));
	}
}

// apply scale then position
void Primitive::SetTransform(const PrimitiveTransform& transform, bool resetTransform)
{
	if (resetTransform)
		this->transform = glm::mat4(1.0f);
	this->transform = glm::translate(this->transform, transform.position);
	this->transform = glm::scale(this->transform, glm::vec3(transform.scale.x, transform.scale.y, 1.0f));
}

void Primitive::SetColor(const glm::vec3& color)
{
	ShaderBind bind(*program);
	program->SetVec3("color", color);
}

void Primitive::Draw() const
{
	ShaderBind bind(*program);
	program->SetMat4("model", this->transform);

	VertexArrayBind vaoBind(*vao);
	BufferBind vboBind(*vbo);
	BufferBind eboBind(*ebo);

	glDrawElements(
		GL_TRIANGLES,
		indexCount,
		GL_UNSIGNED_INT,
		nullptr
	);
}