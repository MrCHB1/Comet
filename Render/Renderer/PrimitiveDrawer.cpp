#include "PrimitiveDrawer.h"

const char* primVert = "#version 330\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aUv;\n"
"uniform mat4 model;\n"
"out vec2 uv;\n"
"void main() {\n"
"    uv = aUv;\n"
"    gl_Position = model * vec4(aPos, 0.0, 1.0);\n"
"    gl_Position.xy = gl_Position.xy * 2.0 - 1.0;\n"
"}";

const char* primFrag = "#version 330\n"
"in vec2 uv;\n"
"out vec4 fragColor;\n"
"uniform vec3 color;\n"
"void main() {\n"
"    fragColor = vec4(color, 1.0);\n"
"}";

Primitive::Primitive(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	static_assert(sizeof(Vertex) == 16);

	indexCount = (GLsizei)indices.size();

	program = ShaderProgram::Create(primVert, primFrag);
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
	this->transform = glm::scale(this->transform, glm::vec3(transform.scale.x, transform.scale.y, 1.0f));
	this->transform = glm::translate(this->transform, transform.position);
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