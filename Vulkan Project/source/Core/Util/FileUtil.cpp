#include "FileUtil.h"
#include <fstream>

namespace FUtil {
	
	std::vector<int8> file_read_binary(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file.");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<int8> buffer(fileSize);

		file.seekg(0);
		file.read((char*)buffer.data(), fileSize);

		file.close();
		return buffer;
	}

}