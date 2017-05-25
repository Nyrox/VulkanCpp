#pragma once
#include <Core/Definitions.h>
#include <vector>
#include <sstream>
#include <filesystem>

namespace std {
	namespace filesystem = experimental::filesystem;
}

namespace FUtil {
	std::vector<int8> file_read_binary(const std::string& filename);
	std::stringstream file_read_stringstream(const std::filesystem::path);

}