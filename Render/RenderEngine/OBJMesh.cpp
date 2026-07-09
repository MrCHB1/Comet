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
    std::string err;

    const char* filename = "./assets/models/cm_keywhite.obj";
    const char* mtl_basedir = "./assets/models/";
    bool success_white = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, mtl_basedir, true);
    
    filename = "./assets/models/cm_keyblack.obj";
    bool success_black = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, mtl_basedir, true);
    
    
    bool success = tinyobj::LoadObj(
		&attrib,
		&shapes,
		&materials,
		&err,
        filename,
		mtl_basedir, "./assets/models/"
	);

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
