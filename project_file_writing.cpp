#include <vector>
#include <string>

#include "project_description.hpp"
#include "stools.hpp"
#include "filesystem.hpp"

enum class layout_item_types : uint8_t {
	control, window, glue, generator, layout, texture_layer
};

void layout_to_bytes(layout_level_t const& layout, serialization::out_buffer& buffer) {
	buffer.start_section();
	buffer.write(layout.size_x);
	buffer.write(layout.size_y);
	buffer.write(layout.margin_top);
	buffer.write(layout.margin_bottom);
	buffer.write(layout.margin_left);
	buffer.write(layout.margin_right);
	buffer.write(layout.line_alignment);
	buffer.write(layout.line_internal_alignment);
	buffer.write(layout.type);
	buffer.write(layout.page_animation);
	buffer.write(layout.interline_spacing);
	buffer.write(layout.paged);

	buffer.start_section(); // expansion section
	buffer.write(layout.template_id);
	buffer.finish_section();

	for(auto& m : layout.contents) {
		if(holds_alternative<layout_control_t>(m)) {
			auto& i = get<layout_control_t>(m);

			buffer.write(layout_item_types::control);
			buffer.write(i.name);
			buffer.write(i.abs_x);
			buffer.write(i.abs_y);
			buffer.write(i.absolute_position);
		} else if(holds_alternative<layout_window_t>(m)) {
			auto& i = get<layout_window_t>(m);

			buffer.write(layout_item_types::window);
			buffer.write(i.name);
			buffer.write(i.abs_x);
			buffer.write(i.abs_y);
			buffer.write(i.absolute_position);
		} else if(holds_alternative<layout_glue_t>(m)) {
			auto& i = get<layout_glue_t>(m);

			buffer.write(layout_item_types::glue);
			buffer.write(i.type);
			buffer.write(i.amount);
		} else if(holds_alternative<generator_t>(m)) {
			auto& i = get<generator_t>(m);

			buffer.write(layout_item_types::generator);
			buffer.write(i.name);

			buffer.start_section();
			for(auto& j : i.inserts) {
				buffer.write(j.name);
				buffer.write(j.header);
				buffer.write(j.inter_item_space);
				buffer.write(j.glue);
				buffer.write(j.sortable);
			}
			buffer.finish_section();

		} else if(holds_alternative<sub_layout_t>(m)) {
			auto& i = get<sub_layout_t>(m);

			buffer.write(layout_item_types::layout);
			layout_to_bytes(*i.layout, buffer);
		} else if(holds_alternative<texture_layer_t>(m)) {
			auto& i = get<texture_layer_t>(m);

			buffer.write(layout_item_types::texture_layer);
			buffer.write(i.texture_type);
			buffer.write(i.texture);
		}
	}
	buffer.finish_section();
}

void bytes_to_layout(layout_level_t& layout, serialization::in_buffer& buffer) {
	auto main_section = buffer.read_section();
	main_section.read(layout.size_x);
	main_section.read(layout.size_y);
	main_section.read(layout.margin_top);
	main_section.read(layout.margin_bottom);
	main_section.read(layout.margin_left);
	main_section.read(layout.margin_right);
	main_section.read(layout.line_alignment);
	main_section.read(layout.line_internal_alignment);
	main_section.read(layout.type);
	main_section.read(layout.page_animation);
	main_section.read(layout.interline_spacing);
	main_section.read(layout.paged);

	auto expansion_section = main_section.read_section();
	if(expansion_section)
		expansion_section.read(layout.template_id);

	while(main_section) {
		layout_item_types t;
		main_section.read(t);
		switch(t) {
			case layout_item_types::control:
			{
				layout_control_t temp;
				main_section.read(temp.name);
				main_section.read(temp.abs_x);
				main_section.read(temp.abs_y);
				main_section.read(temp.absolute_position);
				layout.contents.emplace_back(std::move(temp));
			} break;
			case layout_item_types::window:
			{
				layout_window_t temp;
				main_section.read(temp.name);
				main_section.read(temp.abs_x);
				main_section.read(temp.abs_y);
				main_section.read(temp.absolute_position);
				layout.contents.emplace_back(std::move(temp));
			} break;
			case layout_item_types::glue:
			{
				layout_glue_t temp;
				main_section.read(temp.type);
				main_section.read(temp.amount);
				layout.contents.emplace_back(std::move(temp));
			} break;
			case layout_item_types::generator:
			{
				generator_t temp;

				main_section.read(temp.name);

				auto contents = main_section.read_section();
				while(contents) {
					temp.inserts.emplace_back();
					contents.read(temp.inserts.back().name);
					contents.read(temp.inserts.back().header);
					contents.read(temp.inserts.back().inter_item_space);
					contents.read(temp.inserts.back().glue);
					contents.read(temp.inserts.back().sortable);
				}

				layout.contents.emplace_back(std::move(temp));
			} break;
			case layout_item_types::layout:
			{
				sub_layout_t temp;
				temp.layout = std::make_unique<layout_level_t>();
				bytes_to_layout(*temp.layout, main_section);
				layout.contents.emplace_back(std::move(temp));
			} break;
			case layout_item_types::texture_layer:
			{
				texture_layer_t temp;

				main_section.read(temp.texture_type);
				main_section.read(temp.texture);

				layout.contents.emplace_back(std::move(temp));
			}
		}
	}
}

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
		if(win.wrapped.template_id == -1 && win.wrapped.texture.size() > 0) {
			buffer.write(property::texture);
			buffer.write(win.wrapped.texture);
		}
		if(win.wrapped.template_id == -1 && win.wrapped.alternate_bg.size() > 0) {
			buffer.write(property::alternate_bg);
			buffer.write(win.wrapped.alternate_bg);
		}
		if(win.wrapped.template_id == -1 && (win.wrapped.page_left_texture.size() > 0 || win.wrapped.page_right_texture.size() > 0)) {
			buffer.write(property::page_button_textures);
			buffer.write(win.wrapped.page_left_texture);
			buffer.write(win.wrapped.page_right_texture);
			buffer.write(win.wrapped.page_text_color);
		}
		if(win.layout.contents.size() > 0) {
			buffer.write(property::layout_information);
			layout_to_bytes(win.layout, buffer);
		}
		if(win.wrapped.template_id != -1) {
			buffer.write(property::template_type);
			buffer.write(win.wrapped.template_id);
			int16_t tempgs = int16_t(p.grid_size);
			buffer.write(tempgs);
			buffer.write(win.wrapped.auto_close_button);
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
		if(win.wrapped.share_table_highlight) {
			buffer.write(property::share_table_highlight);
			buffer.write(win.wrapped.share_table_highlight);
		}
		if(win.wrapped.table_connection.size() > 0) {
			buffer.write(property::table_connection);
			buffer.write(win.wrapped.table_connection);
		}
		for(auto& dm : win.wrapped.members) {
			buffer.write(property::data_member);
			buffer.write(dm.type);
			buffer.write(dm.name);
		}
		for(auto& alt : win.alternates) {
			buffer.write(property::alternate_set);
			buffer.write(alt.control_name);
			buffer.write(alt.tempalte_id);
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
			if(c.is_lua) {
				buffer.write(property::is_lua);
				buffer.write(c.is_lua);
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
			if(c.ttype != template_project::template_type::none) {
				buffer.write(property::template_type);
				buffer.write(c.template_id);
				buffer.write(c.ttype);
			}
			if(c.icon_id != -1) {
				buffer.write(property::icon);
				buffer.write(c.icon_id);
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
			if(c.hover_activation) {
				buffer.write(property::hover_activation);
				buffer.write(c.hover_activation);
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
			if(c.hotkey.size() > 0) {
				buffer.write(property::hotkey);
				buffer.write(c.hotkey);
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
			if(c.table_connection.size() > 0) {
				buffer.write(property::table_connection);
				buffer.write(c.table_connection);
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
		auto table_list = tables_in_window(p, win);
		for(auto t : table_list) {
			auto tdef_name = ".tab" + t->name;
			buffer.start_section(); // essential section
			buffer.write(tdef_name);
			buffer.write(t->ascending_sort_icon);
			buffer.write(t->descending_sort_icon);
			buffer.write(t->divider_color);
			buffer.finish_section();
			buffer.start_section(); // col section
			for(auto& tc : t->table_columns) {
				buffer.write(tc.display_data.header_key);
				buffer.write(tc.display_data.header_tooltip_key);
				buffer.write(tc.display_data.cell_tooltip_key);
				buffer.write(tc.display_data.width);
				buffer.write(tc.display_data.cell_text_color);
				buffer.write(tc.display_data.header_text_color);
				buffer.write(tc.display_data.text_alignment);
			}
			buffer.finish_section();
		}
		buffer.finish_section();
	}
	for(auto& tab : p.tables) {
		buffer.start_section();

		buffer.start_section();
		std::string table_type = ".TABLE";
		buffer.write(table_type);
		buffer.write(tab.name);
		buffer.write(tab.ascending_sort_icon);
		buffer.write(tab.descending_sort_icon);
		buffer.write(tab.highlight_color);
		buffer.write(tab.divider_color);
		buffer.write(tab.has_highlight_color);
		buffer.finish_section();

		buffer.start_section();
		for(auto& tc : tab.table_columns) {
			buffer.write(tc.display_data.header_key);
			buffer.write(tc.display_data.header_tooltip_key);
			buffer.write(tc.display_data.header_texture);
			buffer.write(tc.display_data.cell_tooltip_key);
			buffer.write(tc.display_data.width);
			buffer.write(tc.display_data.cell_text_color);
			buffer.write(tc.display_data.header_text_color);
			buffer.write(tc.display_data.text_alignment);
			buffer.write(tc.internal_data.column_name);
			buffer.write(tc.internal_data.container);
			buffer.write(tc.internal_data.cell_type);
			buffer.write(tc.internal_data.has_dy_header_tooltip);
			buffer.write(tc.internal_data.has_dy_cell_tooltip);
			buffer.write(tc.internal_data.sortable);
			buffer.write(tc.internal_data.header_background);
			buffer.write(tc.internal_data.decimal_alignment);
		}
		buffer.finish_section();

		buffer.finish_section();
	}
}

void make_tables_from_legacy(open_project_t& result) {
	if(result.tables.empty()) {
		for(auto& win : result.windows) {
			for(auto& c : win.children) {
				if(c.table_columns.empty() == false) {
					table_definition tab;
					tab.name = c.name;
					tab.table_columns = c.table_columns;
					result.tables.emplace_back(std::move(tab));
					tab.has_highlight_color = c.has_table_highlight_color;
					tab.highlight_color = c.table_highlight_color;
					tab.divider_color = c.table_divider_color;
				}
			}
		}
	}
}

void make_layout_from_legacy(open_project_t& result) {
	for(auto& win : result.windows) {
		if(win.layout.contents.empty()) {
			for(auto& c : win.children) {
				win.layout.contents.push_back(layout_control_t{ c.name, int16_t(-1), c.x_pos, c.y_pos, true });
			}
		}
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

		auto name = essential_window_section.read<std::string_view>();
		if(name == ".TABLE") { // table info
			table_definition tab;
			essential_window_section.read(tab.name);
			essential_window_section.read(tab.ascending_sort_icon);
			essential_window_section.read(tab.descending_sort_icon);
			essential_window_section.read(tab.highlight_color);
			essential_window_section.read(tab.divider_color);
			essential_window_section.read(tab.has_highlight_color);

			auto optional_section = window_section.read_section();
			while(optional_section) {
				full_col_data tc;
				optional_section.read(tc.display_data.header_key);
				optional_section.read(tc.display_data.header_tooltip_key);
				optional_section.read(tc.display_data.header_texture);
				optional_section.read(tc.display_data.cell_tooltip_key);
				optional_section.read(tc.display_data.width);
				optional_section.read(tc.display_data.cell_text_color);
				optional_section.read(tc.display_data.header_text_color);
				optional_section.read(tc.display_data.text_alignment);
				optional_section.read(tc.internal_data.column_name);
				optional_section.read(tc.internal_data.container);
				optional_section.read(tc.internal_data.cell_type);
				optional_section.read(tc.internal_data.has_dy_header_tooltip);
				optional_section.read(tc.internal_data.has_dy_cell_tooltip);
				optional_section.read(tc.internal_data.sortable);
				optional_section.read(tc.internal_data.header_background);
				optional_section.read(tc.internal_data.decimal_alignment);
				tab.table_columns.emplace_back(std::move(tc));
			}
			result.tables.emplace_back(std::move(tab));
		} else {
			window_element_wrapper_t win;
			win.wrapped.name = name;
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
				} else if(ptype == property::page_button_textures) {
					essential_window_section.read(win.wrapped.page_left_texture);
					essential_window_section.read(win.wrapped.page_right_texture);
					essential_window_section.read(win.wrapped.page_text_color);
				} else if(ptype == property::layout_information) {
					bytes_to_layout(win.layout, essential_window_section);
				} else if(ptype == property::template_type) {
					essential_window_section.read(win.wrapped.template_id);
					essential_window_section.read(win.wrapped.stored_gird_size);
					essential_window_section.read(win.wrapped.auto_close_button);
				} else {
					abort();
				}
			}

			auto optional_section = window_section.read_section();

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
				} else if(ptype == property::share_table_highlight) {
					optional_section.read(win.wrapped.share_table_highlight);
				} else if(ptype == property::table_connection) {
					optional_section.read(win.wrapped.table_connection);
				} else if(ptype == property::data_member) {
					data_member m;
					optional_section.read(m.type);
					optional_section.read(m.name);
					win.wrapped.members.push_back(m);
				} else if(ptype == property::alternate_set) {
					template_alternate m;
					optional_section.read(m.control_name);
					optional_section.read(m.tempalte_id);
					win.alternates.push_back(m);
				} else {
					abort();
				}
			}

			while(window_section) {
				auto essential_child_section = window_section.read_section();
				std::string_view name = essential_child_section.read<std::string_view>();
				if(name.starts_with(".tab")) {
					auto optional_child_section = window_section.read_section(); // discard
				} else {
					ui_element_t c;
					c.name = name;
					essential_child_section.read(c.x_pos);
					essential_child_section.read(c.y_pos);
					essential_child_section.read(c.x_size);
					essential_child_section.read(c.y_size);

					while(essential_child_section) {
						auto ptype = essential_child_section.read< property>();
						if(ptype == property::text_color) {
							essential_child_section.read(c.text_color);
						} else if(ptype == property::is_lua) {
							essential_child_section.read(c.is_lua);
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
						} else if(ptype == property::icon) {
							essential_child_section.read(c.icon_id);
						} else if(ptype == property::template_type) {
							essential_child_section.read(c.template_id);
							essential_child_section.read(c.ttype);
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
						} else if(ptype == property::hover_activation) {
							optional_child_section.read(c.hover_activation);
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
						} else if(ptype == property::hotkey) {
							optional_child_section.read(c.hotkey);
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
						} else if(ptype == property::table_connection) {
							optional_child_section.read(c.table_connection);
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
						} else if(ptype == property::tooltip_text_key) {
							optional_child_section.read(c.tooltip_text_key);
						} else if(ptype == property::text_key) {
							optional_child_section.read(c.text_key);
						} else if(ptype == property::template_type) {
							optional_child_section.read(c.template_id);
							optional_child_section.read(c.ttype);
						} else {
							abort();
						}
					}

					win.children.push_back(std::move(c));
				}
			}
			result.windows.push_back(std::move(win));
		}
	}

	make_tables_from_legacy(result);
	make_layout_from_legacy(result);
	return result;
}
