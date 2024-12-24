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
			if(c.no_grid) {
				buffer.write(property::ignore_rtl);
				buffer.write(c.ignore_rtl);
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
			for(auto& dm : c.members) {
				buffer.write(property::data_member);
				buffer.write(dm.type);
				buffer.write(dm.name);
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
				} else {
					abort();
				}
			}

			auto optional_child_section = window_section.read_section();
			while(optional_child_section) {
				auto ptype = optional_child_section.read< property>();
				if(ptype == property::background) {
					optional_child_section.read(c.background);
				} else if(ptype == property::rectangle_color) {
					optional_child_section.read(c.rectangle_color);
				} else if(ptype == property::no_grid) {
					optional_child_section.read(c.no_grid);
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
				} else if(ptype == property::data_member) {
					data_member m;
					optional_child_section.read(m.type);
					optional_child_section.read(m.name);
					c.members.push_back(m);
				} else if(ptype == property::texture) {
					optional_child_section.read(c.texture);
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
