#pragma once

#include <string>
#include <vector>

class OBJMesh
{
public:
	// Immediately loads a mesh upon initialization
	OBJMesh(const std::string& path)
	{
		this->path = path;
	}
	~OBJMesh() = default;

	bool LoadOBJ();
	std::vector<float> vertices;
	std::vector<uint32_t> indices;

private:
	std::string path;
};