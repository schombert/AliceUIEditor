#pragma once
#include <vector>
#include <unordered_map>
#include "filesystem.hpp"

namespace asvg {

class svg_instance {
public:
	uint32_t texture_handle = 0;
	svg_instance() { }
	svg_instance(char const* bytes, int32_t sx, int32_t sy);
	svg_instance(svg_instance&& other) noexcept;
	svg_instance(svg_instance const& other) noexcept {
		std::abort();
	}
	svg_instance& operator=(svg_instance&& other) noexcept;
	svg_instance& operator=(svg_instance const& other) noexcept {
		std::abort();
	}
	~svg_instance() noexcept;
};

enum class dimension_relative : uint8_t {
	height, width, smaller, larger, diagonal, pixel
};

struct affine_replacement {
	uint32_t start_position = 0;
	uint32_t end_position = 0;
	float scale = 1.0f;
	float offset = 0.0f;
	dimension_relative dimension = dimension_relative::width;
	bool emit_quotes = false;
};

class file_bank {
public:
	std::wstring root_directory;
	std::unordered_map<std::string_view, std::vector<char>> file_contents;
	std::pair<void const*, int> get_file_data(std::string_view file_name);
};

class common_file_bank {
public:
	static file_bank bank;
};

class svg {
public:
	std::unordered_map<uint64_t, svg_instance> renders;
	std::vector<char> svg_data;
	std::vector<affine_replacement> replacements;
	int32_t base_width = 1;
	int32_t base_height = 1;
public:
	svg() { }
	svg(char const* data, size_t count, int32_t base_width, int32_t base_height);
	svg(svg&& other) noexcept = default;
	svg& operator=(svg&& other) noexcept = default;

	uint32_t make_new_render(float size_x, float size_y, int32_t grid_size, float scale, float r = 0.0f, float g = 0.0f, float b = 0.0f);
	void release_renders();
	uint32_t get_render(float size_x, float size_y, int32_t grid_size, float scale, float r = 0.0f, float g = 0.0f, float b = 0.0f);
	uint32_t try_get_render(float size_x, float size_y, int32_t grid_size, float r = 0.0f, float g = 0.0f, float b = 0.0f);
};

class simple_svg {
public:
	std::unordered_map<uint64_t, svg_instance> renders;
	std::vector<char> svg_data;
public:
	simple_svg() {
	}
	simple_svg(char const* data, size_t count);
	simple_svg(simple_svg&& other) noexcept = default;
	simple_svg& operator=(simple_svg&& other) noexcept = default;
	uint32_t make_new_render(int32_t size_x, int32_t size_y, float scale, float r = 0.0f, float g = 0.0f, float b = 0.0f);
	void release_renders();
	uint32_t get_render(int32_t size_x, int32_t size_y, float scale, float r = 0.0f, float g = 0.0f, float b = 0.0f);
	uint32_t try_get_render(int32_t size_x, int32_t size_y, float r = 0.0f, float g = 0.0f, float b = 0.0f);
};


}
