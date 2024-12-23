#pragma once
#include <vector>
#include <string>
#include <stdint.h>
#include <unordered_map>
#include "project_description.hpp"

namespace generator {

struct found_code {
	std::string text;
	bool used = false;
};

struct code_snippets {
	std::unordered_map<std::string, found_code> found_code;
	std::string lost_code;
};

code_snippets extract_snippets(char const* data, size_t size);
std::string generate_project_code(open_project_t& proj, code_snippets& old_code);
}
