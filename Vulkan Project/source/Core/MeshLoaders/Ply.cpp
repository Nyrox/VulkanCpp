#include "Ply.h"
#include <Core/Util/FileUtil.h>
#include <Core/Util/StringUtil.h>

namespace MeshLoaders {
	
	namespace {
		struct PlyHeader {
			uint32 vertexCount = 0;
		};

		// Loads the header section of a .ply file
		// Warning: If whatever is in the stream is not a file header this function will read throughout the entire stream
		// never hitting the end_header token, so if you are unsure you should double check
		PlyHeader load_ply_header(std::stringstream& stream) {
			PlyHeader header;
			std::string line;

			while (std::getline(stream, line)) {
				auto tokens = StrUtil::string_split(line, ' ');
				
				if (tokens[0] == "element") {
					if (tokens[1] == "vertex") {
						header.vertexCount = std::stoul(tokens[2]);
					}
				}
				else if (tokens[0] == "end_header") {
					return header;
				}
			}
		}

		// Loads the vertex section of a .ply file
		void load_ply_vertices(std::stringstream& stream, uint32 vertexCount, std::vector<Vertex>& verticesOut) {
			uint32 vertexIndex = 0;
			std::string line;

			while (vertexIndex < vertexCount && std::getline(stream, line)) {
				auto tokens = StrUtil::string_split(line, ' ');

				std::vector<float> values(tokens.size());
				for (int i = 0; i < tokens.size(); i++) {
					values[i] = std::stof(tokens[i]);
				}

				auto position = glm::vec3(values[0], values[1], values[2]);
				auto normal = glm::vec3(values[3], values[4], values[5]);
				auto uv = glm::vec2(values[6], values[7]);

				verticesOut[vertexIndex] = { position, normal, uv };
				vertexIndex++;
			}
		}

		// Loads the face section of a .ply file
		void load_ply_faces(std::stringstream& stream, std::vector<uint32>& indices) {
			uint32 faceIndex = 0;
			std::string line;

			while (std::getline(stream, line)) {
				auto tokens = StrUtil::string_split(line, ' ');

				std::vector<uint32> values(tokens.size());
				for (int i = 0; i < tokens.size(); i++) {
					values[i] = std::stoul(tokens[i]);
				}

				switch (values[0]) {
				case 3:
					indices.insert(indices.end(), { values[1], values[2], values[3] });
					break;
				default:
					throw std::runtime_error("Loading .ply file failed. Can't load faces with " + std::to_string(values[0]) + " vertices.");
				}
			}
		}
	}

	Mesh load_ply(std::filesystem::path path) {
		auto stream = FUtil::file_read_stringstream(path);

		auto header = load_ply_header(stream);
		if (header.vertexCount == 0) throw std::runtime_error("Failed to load mesh: " + path.string() + "\nHeader did not contain a vertex count. Are you sure it's a .ply file?");
	
		std::vector<Vertex> vertices(header.vertexCount);
		std::vector<uint32_t> indices;

		load_ply_vertices(stream, header.vertexCount, vertices);
		load_ply_faces(stream, indices);

		return Mesh { vertices, indices };
	}

}