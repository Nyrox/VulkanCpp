#pragma once
#include <string>
#include <vector>

namespace StrUtil {
	// Splits a string into substrings seperated by `needles`
	extern std::vector<std::string> string_split(const std::string& haystack, std::initializer_list<char> needles);
	extern std::vector<std::string> string_split(const std::string& haystack, char delim);

}