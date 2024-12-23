#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellscalingapi.h>
#include <string>

namespace fs {

std::wstring pick_existing_file(std::wstring extension);
std::wstring pick_new_file(std::wstring extension);
std::wstring pick_directory(std::wstring const& default_folder);

class file {
	HANDLE file_handle = INVALID_HANDLE_VALUE;
	HANDLE mapping_handle = nullptr;
	struct {
		char const* data = nullptr;
		uint32_t file_size = 0;
	} contents;
public:
	file(std::wstring const& full_path);
	file(file const& other) = delete;
	file(file&& other) noexcept;
	void operator=(file const& other) = delete;
	void operator=(file&& other) noexcept;
	~file();

	auto content() const noexcept {
		return contents;
	}
};

void write_file(std::wstring const& full_path, char const* file_data, uint32_t file_size);
std::wstring utf8_to_native(std::string_view str);
std::string native_to_utf8(std::wstring_view str);

}
