#pragma once
#include <Core/Definitions.h>
#include <Core/Render/Vertex.h>

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32> indices;
};