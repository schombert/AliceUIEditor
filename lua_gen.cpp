#include <cstdint>
#include <cstdio>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "project_description.hpp"
#include "stools.hpp"
#include "filesystem.hpp"

open_project_t bytes_to_project(serialization::in_buffer& buffer);

#ifdef WIN32
#else
#endif

#ifdef WIN32
#define native_string std::wstring
#define NATIVE(X) L##X
#define NATIVE_M(X) NATIVE(X)
#define NATIVE_DIR_SEPARATOR L'\\'
#define NATIVE_DIR_SEPARATORS L"\\"
#else
#define native_string std::string
#define NATIVE(X) X
#define NATIVE_M(X) NATIVE(X)
#define NATIVE_DIR_SEPARATOR '/'
#define NATIVE_DIR_SEPARATORS "/"
#endif

constexpr std::string ui_prefix = "UI_LOGIC";

inline std::string table_declaration(std::string project_name, std::string window_name, std::string control_name) {
	return ui_prefix + "." + project_name + "." + window_name + "." + control_name;
}

struct hand_written_sections {
	std::string header;
	std::map<std::string, std::string> sections;

	std::string lost_code;
};

constexpr uint32_t ac(uint32_t current, char item) {
	return (current << 8) + (uint32_t)item;
}

constexpr std::string header_end_str = "@HDR";
constexpr char drop_line = '$';
constexpr char stop_token = '!';

constexpr uint32_t header_end = ac(ac(ac(ac(0, '@'), 'H'), 'D'), 'R');

constexpr std::string user_section_start_str = "--@<";
constexpr std::string user_section_end_str =  "--@>";

constexpr uint32_t user_section_start = ac(ac(ac(ac(0, '-'), '-'), '@'), '<');
constexpr uint32_t user_section_end = ac(ac(ac(ac(0, '-'), '-'), '@'), '>');

hand_written_sections parse_existing_code(open_project_t& input, std::string code) {
	hand_written_sections result;

	std::vector<char> queue;
	uint32_t last_token;

	uint32_t cursor = 0;

	while (last_token != header_end && cursor < code.size()) {
		last_token = ac(last_token, code[cursor]);
		cursor++;
	}

	result.header = code.substr(0, cursor);

	if (last_token != header_end) {
		printf("%s", "Header stop token is not found");
		return result;
	}

	while (cursor < code.size()) {
		while (last_token != user_section_start && cursor < code.size()) {
			last_token = ac(last_token, code[cursor]);
			cursor++;
		}

		uint32_t start_user_section = cursor - 4;

		if (last_token != user_section_start) break;
		std::string key = "";
		while (code[cursor] != stop_token && cursor < code.size()) {
			key += code[cursor];
			cursor++;
		}
		if (code[cursor] != stop_token) break;


		while (last_token != user_section_end && cursor < code.size()) {
			last_token = ac(last_token, code[cursor]);
			cursor++;
		}

		if (last_token != user_section_end) break;

		result.sections[key] = code.substr(start_user_section, cursor - start_user_section);
	}

	return result;
}

std::string project_to_lua(open_project_t& input, std::string generated_code) {
	auto handwritten = parse_existing_code(input, generated_code);

	auto project_name = fs::native_to_utf8(input.project_name);
	std::string tables = ui_prefix + "." +project_name + " = {}\n";

	std::string actual_code = "\n";

	auto push_control_method = [&](std::string method_namespace, std::string method) {
		auto key = method_namespace + "." + method;
		actual_code += "function " + key + "()\n";
		auto it = handwritten.sections.find(key);
		if (it != handwritten.sections.end()) {
			actual_code += it->second;
			actual_code += "\n";
		} else {
			actual_code += user_section_start_str + key + stop_token + "\n";
			actual_code += user_section_end_str + "\n";
		}
		actual_code += "end\n\n";
	};

	// Declare tables to store UI functions
	for (size_t win_idx = 0; win_idx < input.windows.size(); win_idx++) {
		auto& win = input.windows[win_idx];
		tables += ui_prefix + "." + fs::native_to_utf8(input.project_name) + "." + win.wrapped.name + " = {}\n";
		for (size_t control_idx = 0; control_idx < win.children.size(); control_idx++) {
			auto& control = win.children[control_idx];
			auto function_namespace = table_declaration(
				project_name,
				win.wrapped.name,
				control.name
			);
			tables += function_namespace + " = {}\n";

			// auto
			if (control.dynamic_text) {
				push_control_method(function_namespace, "text");
			}
			if (control.left_click_action) {
				push_control_method(function_namespace, "left_click");
			}
			if (control.right_click_action) {
				push_control_method(function_namespace, "right_click");
			}
			if (control.shift_click_action) {
				push_control_method(function_namespace, "shift_click");
			}
		}
	}


	return handwritten.header + "\n" + "\n" + tables + "\n"  + actual_code;
}

#ifdef WIN32
int wmain(int argc, wchar_t *argv[]) {
	if (argc < 3) {
		printf("Required inputs:\nPath to .aui file\nPath to generated .lua file");
		return 1;
	}


	native_string new_file { argv[1] };
	if (new_file.length() == 0) {
		printf("Invalid filename");
		return 1;
	}

	native_string generated_file_path { argv[2] };
	if (new_file.length() == 0) {
		printf("Invalid filename");
		return 1;
	}

	std::string result;

	{
		auto breakpt = new_file.find_last_of(NATIVE_DIR_SEPARATOR);
		auto rem = new_file.substr(breakpt + 1);
		auto ext_pos = rem.find_last_of(L'.');
		fs::file loaded_file{ new_file };
		serialization::in_buffer file_content{ loaded_file.content().data, loaded_file.content().file_size };
		auto open_project = bytes_to_project(file_content);
		open_project.project_name = rem.substr(0, ext_pos);
		open_project.project_directory = new_file.substr(0, breakpt + 1);
		fs::file generated_file {generated_file_path};
		result = project_to_lua(open_project, generated_file.content().data);
	}

	{
		fs::write_file(generated_file_path, result.c_str(), (uint32_t)result.size());
	}
	return 0;
}
#else

#endif