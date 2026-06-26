#pragma once
#include "Render/RenderEngine/OBJMesh.h"`

namespace Models
{
	extern OBJMesh* WhiteKeyMesh;
	extern OBJMesh* BlackKeyMesh;

	void LoadModels();
	void UnloadModels();
}