#include "StringUtil.h"
#include <Core/Definitions.h>

namespace StrUtil {
	std::vector<std::string> string_split(const std::string& haystack, std::initializer_list<char> delims) {
		std::vector<std::string> tokens;
		uint32 prev = 0, pos = 0;

		do {
			for (auto& it : delims) {
				pos = haystack.find(it, prev);
				if (pos != std::string::npos) break;
			}

			if (pos == std::string::npos) pos = haystack.length();
			auto token = haystack.substr(prev, pos - prev);
			if (!token.empty()) tokens.push_back(token);
			prev = pos + 1;
		} while (pos < haystack.length() && prev < haystack.length());

		return tokens;
	}

	std::vector<std::string> string_split(const std::string& haystack, char delim) { return string_split(haystack, { delim }); }

}