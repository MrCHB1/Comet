#include "OBJMesh.h"

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <iostream>

bool OBJMesh::LoadOBJ()
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn, err;

	bool success = tinyobj::LoadObj(
		&attrib,
		&shapes,
		&materials,
		&warn,
		&err,
		path.c_str()
	);

	if (!warn.empty())
		std::cout << warn << std::endl;

	if (!err.empty())
		std::cerr << err << std::endl;

	if (!success)
		return false;

	vertices.clear();
	indices.clear();

	for (size_t i = 0; i < attrib.vertices.size(); i+=3)
	{
		vertices.push_back(attrib.vertices[i + 0]);
		vertices.push_back(attrib.vertices[i + 1]);
		vertices.push_back(attrib.vertices[i + 2]);
	}

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			indices.push_back(index.vertex_index);
		}
	}

	return true;
}