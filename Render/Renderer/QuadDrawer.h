#pragma once

#include "PrimitiveDrawer.h"

class Quad : public Primitive
{
public:
	Quad() : Primitive(
		{
			{ {0.0f, 1.0f}, {0.0f, 1.0f} },
			{ {1.0f, 1.0f}, {1.0f, 1.0f} },
			{ {1.0f, 0.0f}, {1.0f, 0.0f} },
			{ {0.0f, 0.0f}, {0.0f, 0.0f} }
		},
		{
			0,1,3,
			1,2,3
		}
	) { }
};