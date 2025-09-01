#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include "texture.hpp"

enum class background_type : uint8_t {
	none,
	texture,
	bordered_texture,
	existing_gfx,
	linechart,
	stackedbarchart,
	colorsquare,
	flag,
	table_columns,
	table_headers,
	progress_bar,
	icon_strip,
	doughnut,
	border_texture_repeat,
	textured_corners
};

inline bool background_type_is_textured(background_type bg) {
	if (bg == background_type::bordered_texture) return true;
	if (bg == background_type::border_texture_repeat) return true;
	if (bg == background_type::texture) return true;
	if (bg == background_type::textured_corners) return true;
	if (bg == background_type::icon_strip) return true;
	return false;
}

inline bool background_type_requires_border_width(background_type bg) {
	if (bg == background_type::bordered_texture) return true;
	return false;
}

inline bool background_type_has_frames(background_type bg) {
	if (bg == background_type::icon_strip) return true;
	return false;
}

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
	page_up,
	page_middle
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
	std::string page_left_texture;
	std::string page_right_texture;
	std::string table_connection;
	color3f rectangle_color{ 0.2f, 0.2f, 0.2f };
	ogl::texture ogl_texture;
	int16_t x_size = 0;
	int16_t y_size = 0;
	int16_t x_pos = 0;
	int16_t y_pos = 0;
	int16_t border_size = 0;
	background_type background = background_type::none;
	text_color page_text_color = text_color::black;
	bool no_grid = false;
	bool has_alternate_bg = false;
	bool updates_while_hidden = false;
	bool draggable = false;
	orientation orientation = orientation::upper_left;
	bool ignore_rtl = false;
	bool share_table_highlight = false;
	bool parent_is_layout = false;
};

struct texture_layer_t {
	background_type texture;
	ogl::texture ogl_texture;
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
	share_table_highlight = 43,
	page_button_textures = 44,
	layout_information = 45,
	table_connection = 46,
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
struct table_definition {
	std::string name;
	std::string temp_name;
	std::vector< full_col_data> table_columns;
	std::string ascending_sort_icon;
	std::string descending_sort_icon;
	color4f highlight_color{ 0.0f, 0.0f, 0.0f, 0.0f };
	color3f divider_color{ 0.0f, 0.0f, 0.0f };
	bool has_highlight_color = false;
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
	std::string table_connection;
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

enum class layout_type : uint8_t {
	single_horizontal,
	single_vertical,
	overlapped_horizontal,
	overlapped_vertical,
	mulitline_horizontal,
	multiline_vertical
};
enum class layout_line_alignment : uint8_t {
	leading, trailing, centered
};
enum class glue_type : uint8_t {
	standard,
	at_least,
	line_break,
	page_break,
	glue_don_t_break
};

struct layout_control_t {
	std::string name;
	int16_t cached_index = -1;
	int16_t abs_x = 0;
	int16_t abs_y = 0;
	bool absolute_position = false;
};
struct layout_window_t {
	std::string name;
	int16_t cached_index = -1;
	int16_t abs_x = 0;
	int16_t abs_y = 0;
	bool absolute_position = false;
};
struct layout_glue_t {
	glue_type type = glue_type::standard;
	int16_t amount = 0;
};
/*
design: layout process vs. providing specific items

step 0: container update function
step 1: generator update -- generator internally makes a list, resets any internal data
step 3: layout repeatedly asks generator for next (note: may need to back up), also signaling whether it is at the start of a page/column
	layout calls place function, passing NULL as container pointer, generator pseudo-inserts one or more elements, returns size and glue
step 4: particular page is generated:
	generator told to reset pools
	layout calls place function, passing container pointer and item parity, generator inserts one or more elements, returns size (and glue ?)
*/

struct generator_item {
	std::string name;
	std::string header;
	int16_t cached_index = -1;
	int16_t inter_item_space = 0;
	glue_type glue = glue_type::standard;
	bool sortable;
};
struct generator_t {
	std::string name;
	std::vector<generator_item> inserts;
};
struct layout_level_t;
struct sub_layout_t {
	std::unique_ptr<layout_level_t> layout;

	sub_layout_t() noexcept = default;
	sub_layout_t(sub_layout_t const& o) noexcept {
		std::abort();
	}
	sub_layout_t(sub_layout_t&& o) noexcept = default;
	sub_layout_t& operator=(sub_layout_t&& o) noexcept = default;
	~sub_layout_t() = default;
};

using layout_item = std::variant<std::monostate, layout_control_t, layout_window_t, layout_glue_t, generator_t, texture_layer_t, sub_layout_t>;

struct layout_level_t {
	std::vector<layout_item> contents;
	std::vector<int32_t> page_starts;
	bool open_in_ui = false;
	int16_t size_x = -1;
	int16_t size_y = -1;
	int16_t margin_top = 0;
	int16_t margin_bottom = -1;
	int16_t margin_left = -1;
	int16_t margin_right = -1;
	layout_line_alignment line_alignment = layout_line_alignment::leading;
	layout_line_alignment line_internal_alignment = layout_line_alignment::leading;
	// text_color page_display_color = text_color::black;
	layout_type type = layout_type::single_horizontal;
	animation_type page_animation = animation_type::none;
	uint8_t interline_spacing = 0;
	bool paged = false;
};

struct window_element_wrapper_t {
	window_element_t wrapped;
	std::vector<ui_element_t> children;
	layout_level_t layout;
};

struct open_project_t {
	std::wstring project_name;
	std::wstring project_directory;
	std::wstring source_path;
	int32_t grid_size = 9;
	std::vector<window_element_wrapper_t> windows;
	std::vector<table_definition> tables;
};

inline table_definition const* table_from_name(open_project_t const& proj, std::string const& name) {
	for(auto& t : proj.tables) {
		if(t.name == name) {
			return &t;
		}
	}
	return nullptr;
}
inline table_definition* table_from_name(open_project_t& proj, std::string const& name) {
	for(auto& t : proj.tables) {
		if(t.name == name) {
			return &t;
		}
	}
	return nullptr;
}
inline window_element_wrapper_t const* window_from_name(open_project_t const& proj, std::string const& name) {
	for(auto& t : proj.windows) {
		if(t.wrapped.name == name) {
			return &t;
		}
	}
	return nullptr;
}
inline window_element_wrapper_t* window_from_name(open_project_t& proj, std::string const& name) {
	for(auto& t : proj.windows) {
		if(t.wrapped.name == name) {
			return &t;
		}
	}
	return nullptr;
}
inline std::vector<table_definition const*> tables_in_layout(open_project_t const& proj, layout_level_t const& lvl) {
	std::vector<table_definition const*> result;
	for(auto& c : lvl.contents) {
		if(holds_alternative<generator_t>(c)) {
			auto& i = get<generator_t>(c);
			for(auto& m : i.inserts) {
				if(auto w = window_from_name(proj, m.name); w) {
					if(auto t = table_from_name(proj, w->wrapped.table_connection); t && w->wrapped.table_connection.empty() == false) {
						if(std::find(result.begin(), result.end(), t) == result.end())
							result.push_back(t);
					}
				}
			}
		}
		if(holds_alternative<sub_layout_t>(c)) {
			auto& i = get<sub_layout_t>(c);
			auto sr = tables_in_layout(proj, *i.layout);
			for(auto t : sr) {
				if(std::find(result.begin(), result.end(), t) == result.end())
					result.push_back(t);
			}
		}
	}
	return result;
}
inline std::vector<table_definition*> tables_in_layout(open_project_t& proj, layout_level_t& lvl) {
	std::vector<table_definition*> result;
	for(auto& c : lvl.contents) {
		if(holds_alternative<generator_t>(c)) {
			auto& i = get<generator_t>(c);
			for(auto& m : i.inserts) {
				if(auto w = window_from_name(proj, m.name); w) {
					if(auto t = table_from_name(proj, w->wrapped.table_connection); t && w->wrapped.table_connection.empty() == false) {
						if(std::find(result.begin(), result.end(), t) == result.end())
							result.push_back(t);
					}
				}
			}
		}
		if(holds_alternative<sub_layout_t>(c)) {
			auto& i = get<sub_layout_t>(c);
			auto sr = tables_in_layout(proj, *i.layout);
			for(auto t : sr) {
				if(std::find(result.begin(), result.end(), t) == result.end())
					result.push_back(t);
			}
		}
	}
	return result;
}
inline std::vector<table_definition const*> tables_in_window(open_project_t const& proj, window_element_wrapper_t const& win) {
	return tables_in_layout(proj, win.layout);
}
inline std::vector<table_definition*> tables_in_window(open_project_t& proj, window_element_wrapper_t& win) {
	return tables_in_layout(proj, win.layout);
}


