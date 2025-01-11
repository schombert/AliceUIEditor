#pragma once

#include <string>
#include <vector>
#include <variant>
#include "texture.hpp"

enum class background_type : uint8_t {
	none, texture, bordered_texture, existing_gfx, linechart, stackedbarchart
}; 
enum class aui_text_alignment : uint8_t {
	left, right, center
};
inline constexpr int32_t orientation_bit_offset = 5;
enum class orientation : uint8_t { // 3 bits
	upper_left = (0x00 << orientation_bit_offset),
	upper_right = (0x01 << orientation_bit_offset),
	lower_left = (0x02 << orientation_bit_offset),
	lower_right = (0x03 << orientation_bit_offset),
	upper_center = (0x04 << orientation_bit_offset),
	lower_center = (0x05 << orientation_bit_offset),
	center = (0x06 << orientation_bit_offset)
};

enum class text_color : uint8_t {
	black,
	white,
	red,
	green,
	yellow,
	unspecified,
	light_blue,
	dark_blue,
	orange,
	lilac,
	light_grey,
	dark_red,
	dark_green,
	gold,
	reset,
	brown
};

enum class text_type : uint8_t {
	body,
	header
};

enum class container_type : uint8_t {
	none,
	list,
	grid,
	table
};

enum class animation_type : uint8_t {
	none,
	page_left,
	page_right,
	page_up
};

struct color3f {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	bool operator==(color3f const& o) const noexcept {
		return r == o.r && g == o.g && b == o.b;
	}
	bool operator!=(color3f const& o) const noexcept {
		return !(*this == o);
	}
	color3f operator*(float v) const noexcept {
		return color3f{ r * v, b * v, g * v };
	}
};

struct color4f {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;

	bool operator==(color4f const& o) const noexcept {
		return r == o.r && g == o.g && b == o.b && a == o.a;
	}
	bool operator!=(color4f const& o) const noexcept {
		return !(*this == o);
	}
	color4f operator*(float v) const noexcept {
		return color4f{ r * v, b * v, g * v, a };
	}
};

struct data_member {
	std::string type;
	std::string name;
};

struct window_element_t {
	std::vector< data_member> members;
	std::string name;
	std::string temp_name;
	std::string parent;
	std::string texture;
	std::string alternate_bg;
	color3f rectangle_color{ 1.0f, 0.0f, 0.0f };
	ogl::texture ogl_texture;
	int16_t x_size = 0;
	int16_t y_size = 0;
	int16_t x_pos = 0;
	int16_t y_pos = 0;
	int16_t border_size = 0;
	background_type background = background_type::none;
	bool no_grid = false;
	bool has_alternate_bg = false;
	bool updates_while_hidden = false;
	bool draggable = false;
	orientation orientation = orientation::upper_left;
	bool ignore_rtl = false;
};

enum class property : uint8_t {
	texture = 1,
	tooltip_text_key = 2,
	text_key = 3,
	rectangle_color = 4,
	text_color = 5,
	background = 6,
	no_grid = 7,
	text_align = 8,
	dynamic_element = 9,
	dynamic_tooltip = 10,
	can_disable = 11,
	updates_while_hidden = 12,
	left_click_action = 13,
	right_click_action = 14,
	shift_click_action = 15,
	dynamic_text = 16,
	border_size = 17,
	text_scale = 18,
	text_type = 19,
	container_type = 20,
	child_window = 21,
	list_content = 22,
	data_member = 23,
	has_alternate_bg = 24,
	alternate_bg = 25,
	ignore_rtl = 25,
	draggable = 26,
	table_highlight_color = 27,
	ascending_sort_icon = 28,
	descending_sort_icon = 29,
	row_background_a = 30,
	row_background_b = 31,
	row_height = 32,
	table_insert = 33,
	table_display_column_data = 34,
	table_internal_column_data = 35,
	table_divider_color = 36,
	table_has_per_section_headers = 37,
	animation_type = 38,
	datapoints = 39,
	other_color = 40,
	hover_activation = 41,
	hotkey = 42,
};
enum class table_cell_type : uint8_t {
	spacer = 0, text = 1, container = 2,
};
struct table_internal_column {
	std::string column_name;
	std::string container;
	table_cell_type cell_type = table_cell_type::spacer;
	bool has_dy_header_tooltip = false;
	bool has_dy_cell_tooltip = false;
	bool sortable = false;
	bool header_background = false;
	aui_text_alignment decimal_alignment = aui_text_alignment::center; // center == none
};
struct table_display_column {
	std::string header_key;
	std::string header_tooltip_key;
	std::string header_texture;
	std::string cell_tooltip_key;
	int16_t width = 0;
	text_color cell_text_color = text_color::black;
	text_color header_text_color = text_color::black;
	aui_text_alignment text_alignment = aui_text_alignment::center;
};
struct full_col_data {
	table_internal_column internal_data;
	table_display_column display_data;
};
struct ui_element_t {
	std::vector< data_member> members;
	std::vector< std::string> table_inserts;
	std::vector< full_col_data> table_columns;
	std::string name;
	std::string temp_name;
	std::string texture;
	std::string tooltip_text_key;
	std::string text_key;
	std::string child_window;
	std::string list_content;
	std::string alternate_bg;
	std::string ascending_sort_icon;
	std::string descending_sort_icon;
	std::string row_background_a;
	std::string row_background_b;
	std::string hotkey;
	color4f table_highlight_color{ 0.0f, 0.0f, 0.0f, 0.0f };
	color4f other_color{ 0.0f, 0.0f, 0.0f, 0.0f };
	color3f rectangle_color{ 1.0f, 0.0f, 0.0f };
	color3f table_divider_color{ 0.0f, 0.0f, 0.0f };
	ogl::texture ogl_texture;
	float text_scale = 1.0f;
	float row_height = 2.0f;
	int16_t x_size = 0;
	int16_t y_size = 0;
	int16_t x_pos = 0;
	int16_t y_pos = 0;
	int16_t border_size = 0;
	int16_t datapoints = 100;
	text_color text_color = text_color::black;
	text_type text_type = text_type::body;
	background_type background = background_type::none;
	container_type container_type = container_type::none;
	bool no_grid = false;
	aui_text_alignment text_align = aui_text_alignment::left;
	animation_type animation_type = animation_type::none;
	bool dynamic_element = false;
	bool dynamic_tooltip = false;
	bool can_disable = false;
	bool updates_while_hidden = false;
	bool left_click_action = false;
	bool right_click_action = false;
	bool shift_click_action = false;
	bool dynamic_text = false;
	bool has_alternate_bg = false;
	bool ignore_rtl = false;
	bool has_table_highlight_color = false;
	bool table_has_per_section_headers = false;
	bool hover_activation = false;
};

struct window_element_wrapper_t {
	window_element_t wrapped;
	std::vector<ui_element_t> children;
};

struct open_project_t {
	std::wstring project_name;
	std::wstring project_directory;
	std::wstring source_path;
	int32_t grid_size = 9;
	std::vector<window_element_wrapper_t> windows;
};

