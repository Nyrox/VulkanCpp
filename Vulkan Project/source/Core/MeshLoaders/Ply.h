#pragma once
#include <Core/Render/Mesh.h>
#include <filesystem>

namespace std {
	namespace filesystem = experimental::filesystem;
}

namespace MeshLoaders {
	extern Mesh load_ply(std::filesystem::path path);
}