#include <vector>
#include <string>

#include "project_description.hpp"
#include "stools.hpp"
#include "filesystem.hpp"

void project_to_bytes(open_project_t const& p, serialization::out_buffer& buffer) {
	buffer.start_section(); // header
	buffer.write(p.grid_size);
	buffer.write(p.source_path);
	buffer.finish_section();

	for(auto& win : p.windows) {
		buffer.start_section();

		buffer.start_section(); // essential section
		buffer.write(win.wrapped.name);
		buffer.write(win.wrapped.x_pos);
		buffer.write(win.wrapped.y_pos);
		buffer.write(win.wrapped.x_size);
		buffer.write(win.wrapped.y_size);
		buffer.write(win.wrapped.orientation);

		if(win.wrapped.border_size != 0) {
			buffer.write(property::border_size);
			buffer.write(win.wrapped.border_size);
		}
		if(win.wrapped.texture.size() > 0) {
			buffer.write(property::texture);
			buffer.write(win.wrapped.texture);
		}
		if(win.wrapped.alternate_bg.size() > 0) {
			buffer.write(property::alternate_bg);
			buffer.write(win.wrapped.alternate_bg);
		}
		buffer.finish_section();

		buffer.start_section(); // optional section
		buffer.write(win.wrapped.parent);

		if(win.wrapped.background != background_type::none) {
			buffer.write(property::background);
			buffer.write(win.wrapped.background);
		}
		if(win.wrapped.rectangle_color != color3f{ 1.0f, 0.0f, 0.0f }) {
			buffer.write(property::rectangle_color);
			buffer.write(win.wrapped.rectangle_color);
		}
		if(win.wrapped.has_alternate_bg) {
			buffer.write(property::has_alternate_bg);
			buffer.write(win.wrapped.has_alternate_bg);
		}
		if(win.wrapped.no_grid) {
			buffer.write(property::no_grid);
			buffer.write(win.wrapped.no_grid);
		}
		if(win.wrapped.draggable) {
			buffer.write(property::draggable);
			buffer.write(win.wrapped.draggable);
		}
		if(win.wrapped.updates_while_hidden) {
			buffer.write(property::updates_while_hidden);
			buffer.write(win.wrapped.updates_while_hidden);
		}
		if(win.wrapped.ignore_rtl) {
			buffer.write(property::ignore_rtl);
			buffer.write(win.wrapped.ignore_rtl);
		}
		for(auto& dm : win.wrapped.members) {
			buffer.write(property::data_member);
			buffer.write(dm.type);
			buffer.write(dm.name);
		}
		buffer.finish_section();

		for(auto& c : win.children) {
			buffer.start_section(); // essential section

			buffer.write(c.name);
			buffer.write(c.x_pos);
			buffer.write(c.y_pos);
			buffer.write(c.x_size);
			buffer.write(c.y_size);
			if(c.text_color != text_color::black) {
				buffer.write(property::text_color);
				buffer.write(c.text_color);
			}
			if(c.tooltip_text_key.size() > 0) {
				buffer.write(property::tooltip_text_key);
				buffer.write(c.tooltip_text_key);
			}
			if(c.text_key.size() > 0) {
				buffer.write(property::text_key);
				buffer.write(c.text_key);
			}
			if(c.text_align != aui_text_alignment::left) {
				buffer.write(property::text_align);
				buffer.write(c.text_align);
			}
			if(c.border_size != 0) {
				buffer.write(property::border_size);
				buffer.write(c.border_size);
			}
			if(c.text_type != text_type::body) {
				buffer.write(property::text_type);
				buffer.write(c.text_type);
			}
			if(c.text_scale != 1.0f) {
				buffer.write(property::text_scale);
				buffer.write(c.text_scale);
			}
			if(c.texture.size() > 0) {
				buffer.write(property::texture);
				buffer.write(c.texture);
			}
			if(c.alternate_bg.size() > 0) {
				buffer.write(property::alternate_bg);
				buffer.write(c.alternate_bg);
			}
			if(c.has_table_highlight_color && c.container_type == container_type::table) {
				buffer.write(property::table_highlight_color);
				buffer.write(c.table_highlight_color);
			}
			if(c.ascending_sort_icon.size() > 0) {
				buffer.write(property::ascending_sort_icon);
				buffer.write(c.ascending_sort_icon);
			}
			if(c.descending_sort_icon.size() > 0) {
				buffer.write(property::descending_sort_icon);
				buffer.write(c.descending_sort_icon);
			}
			if(c.row_background_a.size() > 0) {
				buffer.write(property::row_background_a);
				buffer.write(c.row_background_a);
			}
			if(c.row_background_b.size() > 0) {
				buffer.write(property::row_background_b);
				buffer.write(c.row_background_b);
			}
			if(c.row_height != 2.0f) {
				buffer.write(property::row_height);
				buffer.write(c.row_height);
			}
			if(c.table_divider_color != color3f{ 0.0f, 0.0f, 0.0f}) {
				buffer.write(property::table_divider_color);
				buffer.write(c.table_divider_color);
			}
			if(c.other_color != color4f{ 0.0f, 0.0f, 0.0f, 0.0f } && c.container_type != container_type::table) {
				buffer.write(property::other_color);
				buffer.write(c.other_color);
			}
			for(auto& tc : c.table_columns) {
				buffer.write(property::table_display_column_data);
				buffer.write(tc.display_data.header_key);
				buffer.write(tc.display_data.header_tooltip_key);
				buffer.write(tc.display_data.header_texture);
				buffer.write(tc.display_data.cell_tooltip_key);
				buffer.write(tc.display_data.width);
				buffer.write(tc.display_data.cell_text_color);
				buffer.write(tc.display_data.header_text_color);
				buffer.write(tc.display_data.text_alignment);
			}
			buffer.finish_section();

			buffer.start_section(); // optional section
			if(c.background != background_type::none) {
				buffer.write(property::background);
				buffer.write(c.background);
			}
			if(c.rectangle_color != color3f{ 1.0f, 0.0f, 0.0f }) {
				buffer.write(property::rectangle_color);
				buffer.write(c.rectangle_color);
			}
			if(c.no_grid) {
				buffer.write(property::no_grid);
				buffer.write(c.no_grid);
			}
			if(c.ignore_rtl) {
				buffer.write(property::ignore_rtl);
				buffer.write(c.ignore_rtl);
			}
			if(c.table_has_per_section_headers) {
				buffer.write(property::table_has_per_section_headers);
				buffer.write(c.table_has_per_section_headers);
			}
			if(c.dynamic_element) {
				buffer.write(property::dynamic_element);
				buffer.write(c.dynamic_element);
			}
			if(c.dynamic_tooltip) {
				buffer.write(property::dynamic_tooltip);
				buffer.write(c.dynamic_tooltip);
			}
			if(c.can_disable) {
				buffer.write(property::can_disable);
				buffer.write(c.can_disable);
			}
			if(c.updates_while_hidden) {
				buffer.write(property::updates_while_hidden);
				buffer.write(c.updates_while_hidden);
			}
			if(c.left_click_action) {
				buffer.write(property::left_click_action);
				buffer.write(c.left_click_action);
			}
			if(c.right_click_action) {
				buffer.write(property::right_click_action);
				buffer.write(c.right_click_action);
			}
			if(c.shift_click_action) {
				buffer.write(property::shift_click_action);
				buffer.write(c.shift_click_action);
			}
			if(c.dynamic_text) {
				buffer.write(property::dynamic_text);
				buffer.write(c.dynamic_text);
			}
			if(c.container_type != container_type::none) {
				buffer.write(property::container_type);
				buffer.write(c.container_type);
			}
			if(c.datapoints != 100) {
				buffer.write(property::datapoints);
				buffer.write(c.datapoints);
			}
			if(c.child_window.size() > 0) {
				buffer.write(property::child_window);
				buffer.write(c.child_window);
			}
			if(c.list_content.size() > 0) {
				buffer.write(property::list_content);
				buffer.write(c.list_content);
			}
			if(c.has_alternate_bg) {
				buffer.write(property::has_alternate_bg);
				buffer.write(c.has_alternate_bg);
			}
			if(c.animation_type != animation_type::none) {
				buffer.write(property::animation_type);
				buffer.write(c.animation_type);
			}
			for(auto& dm : c.members) {
				buffer.write(property::data_member);
				buffer.write(dm.type);
				buffer.write(dm.name);
			}
			for(auto& tc : c.table_columns) {
				buffer.write(property::table_internal_column_data);
				buffer.write(tc.internal_data.column_name);
				buffer.write(tc.internal_data.container);
				buffer.write(tc.internal_data.cell_type);
				buffer.write(tc.internal_data.has_dy_header_tooltip);
				buffer.write(tc.internal_data.has_dy_cell_tooltip);
				buffer.write(tc.internal_data.sortable);
				buffer.write(tc.internal_data.header_background);
				buffer.write(tc.internal_data.decimal_alignment);
			}
			for(auto& i : c.table_inserts) {
				buffer.write(property::table_insert);
				buffer.write(i);
			}
			buffer.finish_section();
		}
		buffer.finish_section();
	}
}

open_project_t bytes_to_project(serialization::in_buffer& buffer) {
	open_project_t result;
	auto header_section = buffer.read_section();
	header_section.read(result.grid_size);
	header_section.read(result.source_path);

	while(buffer) {
		auto window_section = buffer.read_section(); // essential section

		auto essential_window_section = window_section.read_section(); // essential section
		window_element_wrapper_t win;

		essential_window_section.read(win.wrapped.name);
		essential_window_section.read(win.wrapped.x_pos);
		essential_window_section.read(win.wrapped.y_pos);
		essential_window_section.read(win.wrapped.x_size);
		essential_window_section.read(win.wrapped.y_size);
		essential_window_section.read(win.wrapped.orientation);
		while(essential_window_section) {
			auto ptype = essential_window_section.read< property>();
			if(ptype == property::border_size) {
				essential_window_section.read(win.wrapped.border_size);
			} else if(ptype == property::texture) {
				essential_window_section.read(win.wrapped.texture);
			} else if(ptype == property::alternate_bg) {
				essential_window_section.read(win.wrapped.alternate_bg);
			} else {
				abort();
			}
		}

		auto optional_section = window_section.read_section(); // essential section

		optional_section.read(win.wrapped.parent);
		while(optional_section) {
			auto ptype = optional_section.read< property>();
			if(ptype == property::background) {
				optional_section.read(win.wrapped.background);
			} else if(ptype == property::rectangle_color) {
				optional_section.read(win.wrapped.rectangle_color);
			} else if(ptype == property::no_grid) {
				optional_section.read(win.wrapped.no_grid);
			} else if(ptype == property::ignore_rtl) {
				optional_section.read(win.wrapped.ignore_rtl);
			} else if(ptype == property::draggable) {
				optional_section.read(win.wrapped.draggable);
			} else if(ptype == property::updates_while_hidden) {
				optional_section.read(win.wrapped.updates_while_hidden);
			} else if(ptype == property::texture) {
				optional_section.read(win.wrapped.texture);
			} else if(ptype == property::has_alternate_bg) {
				optional_section.read(win.wrapped.has_alternate_bg);
			} else if(ptype == property::data_member) {
				data_member m;
				optional_section.read(m.type);
				optional_section.read(m.name);
				win.wrapped.members.push_back(m);
			} else {
				abort();
			}
		}

		while(window_section) {
			ui_element_t c;
			auto essential_child_section = window_section.read_section();

			essential_child_section.read(c.name);
			essential_child_section.read(c.x_pos);
			essential_child_section.read(c.y_pos);
			essential_child_section.read(c.x_size);
			essential_child_section.read(c.y_size);

			while(essential_child_section) {
				auto ptype = essential_child_section.read< property>();
				if(ptype == property::text_color) {
					essential_child_section.read(c.text_color);
				} else if(ptype == property::texture) {
					essential_child_section.read(c.texture);
				} else if(ptype == property::alternate_bg) {
					essential_child_section.read(c.alternate_bg);
				} else if(ptype == property::text_align) {
					essential_child_section.read(c.text_align);
				} else if(ptype == property::tooltip_text_key) {
					essential_child_section.read(c.tooltip_text_key);
				} else if(ptype == property::text_key) {
					essential_child_section.read(c.text_key);
				} else if(ptype == property::text_type) {
					essential_child_section.read(c.text_type);
				} else if(ptype == property::text_scale) {
					essential_child_section.read(c.text_scale);
				} else if(ptype == property::border_size) {
					essential_child_section.read(c.border_size);
				} else if(ptype == property::table_highlight_color) {
					c.has_table_highlight_color = true;
					essential_child_section.read(c.table_highlight_color);
				} else if(ptype == property::ascending_sort_icon) {
					essential_child_section.read(c.ascending_sort_icon);
				} else if(ptype == property::descending_sort_icon) {
					essential_child_section.read(c.descending_sort_icon);
				} else if(ptype == property::row_background_a) {
					essential_child_section.read(c.row_background_a);
				} else if(ptype == property::row_background_b) {
					essential_child_section.read(c.row_background_b);
				} else if(ptype == property::row_height) {
					essential_child_section.read(c.row_height);
				} else if(ptype == property::table_divider_color) {
					essential_child_section.read(c.table_divider_color);
				} else if(ptype == property::other_color) {
					essential_child_section.read(c.other_color);
				} else if(ptype == property::table_display_column_data) {
					table_display_column tc;
					essential_child_section.read(tc.header_key);
					essential_child_section.read(tc.header_tooltip_key);
					essential_child_section.read(tc.header_texture);
					essential_child_section.read(tc.cell_tooltip_key);
					essential_child_section.read(tc.width);
					essential_child_section.read(tc.cell_text_color);
					essential_child_section.read(tc.header_text_color);
					essential_child_section.read(tc.text_alignment);
					c.table_columns.emplace_back();
					c.table_columns.back().display_data = tc;
				} else {
					abort();
				}
			}
			int32_t col_count = 0;
			auto optional_child_section = window_section.read_section();
			while(optional_child_section) {
				auto ptype = optional_child_section.read< property>();
				if(ptype == property::background) {
					optional_child_section.read(c.background);
				} else if(ptype == property::rectangle_color) {
					optional_child_section.read(c.rectangle_color);
				} else if(ptype == property::no_grid) {
					optional_child_section.read(c.no_grid);
				} else if(ptype == property::table_has_per_section_headers) {
					optional_child_section.read(c.table_has_per_section_headers);
				} else if(ptype == property::ignore_rtl) {
					optional_child_section.read(c.ignore_rtl);
				} else if(ptype == property::dynamic_element) {
					optional_child_section.read(c.dynamic_element);
				} else if(ptype == property::dynamic_tooltip) {
					optional_child_section.read(c.dynamic_tooltip);
				} else if(ptype == property::can_disable) {
					optional_child_section.read(c.can_disable);
				} else if(ptype == property::updates_while_hidden) {
					optional_child_section.read(c.updates_while_hidden);
				} else if(ptype == property::left_click_action) {
					optional_child_section.read(c.left_click_action);
				} else if(ptype == property::right_click_action) {
					optional_child_section.read(c.right_click_action);
				} else if(ptype == property::shift_click_action) {
					optional_child_section.read(c.shift_click_action);
				} else if(ptype == property::dynamic_text) {
					optional_child_section.read(c.dynamic_text);
				} else if(ptype == property::container_type) {
					optional_child_section.read(c.container_type);
				} else if(ptype == property::child_window) {
					optional_child_section.read(c.child_window);
				} else if(ptype == property::list_content) {
					optional_child_section.read(c.list_content);
				} else if(ptype == property::has_alternate_bg) {
					optional_child_section.read(c.has_alternate_bg);
				} else if(ptype == property::animation_type) {
					optional_child_section.read(c.animation_type);
				} else if(ptype == property::datapoints) {
					optional_child_section.read(c.datapoints);
				} else if(ptype == property::data_member) {
					data_member m;
					optional_child_section.read(m.type);
					optional_child_section.read(m.name);
					c.members.push_back(m);
				} else if(ptype == property::table_insert) {
					std::string n;
					optional_child_section.read(n);
					c.table_inserts.push_back(n);
				} else if(ptype == property::texture) {
					optional_child_section.read(c.texture);
				} else if(ptype == property::table_internal_column_data) {
					if(col_count >= int32_t(c.table_columns.size()))
						std::abort();
					table_internal_column tc;
					optional_child_section.read(tc.column_name);
					optional_child_section.read(tc.container);
					optional_child_section.read(tc.cell_type);
					optional_child_section.read(tc.has_dy_header_tooltip);
					optional_child_section.read(tc.has_dy_cell_tooltip);
					optional_child_section.read(tc.sortable);
					optional_child_section.read(tc.header_background);
					optional_child_section.read(tc.decimal_alignment);
					c.table_columns[col_count].internal_data = tc;
					++col_count;
				} else {
					abort();
				}
			}

			win.children.push_back(std::move(c));
		}
		result.windows.push_back(std::move(win));
	}

	return result;
}
