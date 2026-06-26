#include "Models.h"
#include <iostream>

OBJMesh* Models::WhiteKeyMesh = nullptr;
OBJMesh* Models::BlackKeyMesh = nullptr;

void Models::LoadModels()
{
	UnloadModels();

	WhiteKeyMesh = new OBJMesh("./assets/models/cm_keywhite.obj");
	if (!WhiteKeyMesh->LoadOBJ())
		std::cerr << "Loading white key mesh failed! Vertices and indices will be empty" << std::endl;

	BlackKeyMesh = new OBJMesh("./assets/models/cm_keyblack.obj");
	if (!BlackKeyMesh->LoadOBJ())
		std::cerr << "Loading black key mesh failed! Vertices and indices will be empty" << std::endl;
}

void Models::UnloadModels()
{
	if (WhiteKeyMesh != nullptr)
	{
		delete WhiteKeyMesh;
		WhiteKeyMesh = nullptr;
	}

	if (BlackKeyMesh != nullptr)
	{
		delete BlackKeyMesh;
		BlackKeyMesh = nullptr;
	}
}