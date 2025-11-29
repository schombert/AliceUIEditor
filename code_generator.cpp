#include "code_generator.hpp"
#include <string>
#include <string_view>
#include "filesystem.hpp"
#include "project_description.hpp"

namespace generator {

struct comment_result {
	std::string_view label;
	std::string_view line;
	bool was_comment;
};

comment_result analyze_line(char const* data, size_t& read_position, size_t size) {
	auto line_start = read_position;

	while(read_position < size && (data[read_position] == ' ' || data[read_position] == '\t')) {
		++read_position;
	}
	if(read_position + 1 < size && data[read_position] == '/' && data[read_position + 1] == '/') {
		// line is comment
		read_position += 2;
		while(read_position < size && (data[read_position] == ' ' || data[read_position] == '\t')) {
			++read_position;
		}
		if(read_position + 4 < size && data[read_position] == 'B' && data[read_position+1] == 'E' && data[read_position +2] == 'G' && data[read_position + 3] == 'I' && data[read_position + 4] == 'N') { // BEGIN comment
			read_position += 5;
			while(read_position < size && (data[read_position] == ' ' || data[read_position] == '\t')) {
				++read_position;
			}
			auto start_label = read_position;
			while(read_position < size && data[read_position] != ' ' && data[read_position] != '\t' && data[read_position] != '\r' && data[read_position] != '\n') {
				++read_position;
			}
			std::string_view label{ data + start_label, data + read_position };
			while(read_position < size && data[read_position] != '\r' && data[read_position] != '\n') {
				++read_position;
			}
			while(read_position < size && (data[read_position] == '\r' || data[read_position] == '\n')) {
				++read_position;
			}
			return comment_result{ label, std::string_view{ data + line_start, data + read_position }, true };
		} else if(read_position + 2 < size && data[read_position] == 'E' && data[read_position + 1] == 'N' && data[read_position + 2] == 'D' ) { // END comment
			read_position += 3;
			while(read_position < size && data[read_position] != '\r' && data[read_position] != '\n') {
				++read_position;
			}
			while(read_position < size && (data[read_position] == '\r' || data[read_position] == '\n')) {
				++read_position;
			}
			return comment_result{ "END", std::string_view{ data + line_start, data + read_position }, true };
		} else { // other comment type
			while(read_position < size && data[read_position] != '\r' && data[read_position] != '\n') {
				++read_position;
			}
			while(read_position < size && (data[read_position] == '\r' || data[read_position] == '\n')) {
				++read_position;
			}
			return comment_result{"",  std::string_view{ data + line_start, data + read_position }, false };
		}
	} else {
		while(read_position < size && data[read_position] != '\r' && data[read_position] != '\n') {
			++read_position;
		}
		while(read_position < size && (data[read_position] == '\r' || data[read_position] == '\n')) {
			++read_position;
		}
		return comment_result{ "", std::string_view{ data + line_start, data + read_position }, false };
	}
}


code_snippets extract_snippets(char const* data, size_t size) {
	code_snippets result;
	size_t read_position = 0;
	while(read_position < size) {
		auto r = analyze_line(data, read_position, size);
		if(r.was_comment) {
			if(r.label == "LOST-CODE") {
				while(read_position < size) {
					auto line = analyze_line(data, read_position, size);
					if(!line.was_comment) {
						break;
					} else {
						result.lost_code += line.line;
					}
				}
			} else if(r.label == "END") { // misplaced end

			} else {
				found_code found;
				while(read_position < size) {
					auto line = analyze_line(data, read_position, size);
					if(line.was_comment && line.label == "END") {
						break;
					} else {
						found.text += line.line;
					}
				}
				result.found_code.insert_or_assign(std::string(r.label), std::move(found));
			}
		}
	}
	return result;
}

std::string color_to_name(text_color t) {
	switch(t) {
		case text_color::black: return "black";
		case text_color::white: return "white";
		case text_color::red: return "red";
		case text_color::green: return "green";
		case text_color::yellow: return "yellow";
		case text_color::unspecified: return "unspecified";
		case text_color::light_blue: return "light_blue";
		case text_color::dark_blue: return "dark_blue";
		case text_color::orange: return "orange";
		case text_color::lilac: return "lilac";
		case text_color::light_grey: return "light_grey";
		case text_color::dark_red: return "dark_red";
		case text_color::dark_green: return "dark_green";
		case text_color::gold: return "gold";
		case text_color::reset: return "reset";
		case text_color::brown: return "brown";
		default: return "black";
	}
}
std::string alignment_to_name(aui_text_alignment t) {
	switch(t) {
		case aui_text_alignment::left: return "left";
		case aui_text_alignment::right: return "right";
		case aui_text_alignment::center: return "center";
		default: return "left";
	}
}

std::vector<generator_t*> make_generators_list(layout_level_t& lvl) {
	std::vector<generator_t*> result;
	for(auto& m : lvl.contents) {
		if(std::holds_alternative<generator_t>(m)) {
			result.push_back(&(std::get<generator_t>(m)));
		}
		if(std::holds_alternative<sub_layout_t>(m)) {
			auto subr = make_generators_list(*(get<sub_layout_t>(m).layout));
			result.insert(result.end(), subr.begin(), subr.end());
		}
	}
	return result;
}

bool any_layout_paged(layout_level_t& lvl) {
	if(lvl.paged)
		return true;
	for(auto& m : lvl.contents) {
		if(holds_alternative<sub_layout_t>(m)) {
			auto r = any_layout_paged(*get<sub_layout_t>(m).layout);
			if(r) return true;
		}
	}
	return false;
}
bool any_layout_contains_window(layout_level_t& lvl, std::string const& window_name) {
	for(auto& m : lvl.contents) {
		if(holds_alternative<layout_window_t>(m)) {
			if(get<layout_window_t>(m).name == window_name)
				return true;
		} else if(holds_alternative<generator_t>(m)) {
			auto& i = get<generator_t>(m);
			for(auto& j : i.inserts) {
				if(j.name == window_name)
					return true;
				if(j.header == window_name)
					return true;
			}
		} else if(holds_alternative<sub_layout_t>(m)) {
			auto r = any_layout_contains_window(*get<sub_layout_t>(m).layout, window_name);
			if(r) return true;
		}
	}
	return false;
}


bool element_needs_class(ui_element_t const& c) {
	if(is_lua_element(c))
		return false;
	if(!c.members.empty())
		return true;
	switch(c.ttype) {
		case template_project::template_type::label:
			return c.dynamic_text || c.dynamic_tooltip;
		case template_project::template_type::free_icon:
			return c.dynamic_text || c.dynamic_tooltip;
		case template_project::template_type::free_background:
			return c.dynamic_text || c.dynamic_tooltip;
		case template_project::template_type::button:
			return true;
		case template_project::template_type::iconic_button:
			return true;
		case template_project::template_type::mixed_button:
			return true;
		case template_project::template_type::mixed_button_ci:
			return true;
		case template_project::template_type::toggle_button:
			return true;
		case template_project::template_type::table_header:
			return true;
		case template_project::template_type::table_row:
			return true;
		case template_project::template_type::stacked_bar_chart:
			return true;
		case template_project::template_type::table_highlights:
			return true;
		case template_project::template_type::drop_down_control:
			return true;
		case template_project::template_type::edit_control:
			return true;
		default:
			return true;
	}
	return true;
}

std::string element_type_pre_declarations(std::string const& project_name, window_element_wrapper_t const& win, ui_element_t const& c) {
	std::string result;
	if(!element_needs_class(c)) {

	} else {
		result += "struct " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t;\n";
	}
	return result;
}

std::string element_class_name(std::string const& project_name, window_element_wrapper_t const& win, ui_element_t const& c) {
	switch(c.ttype) {
		case template_project::template_type::label:
			if(!c.dynamic_text && !c.dynamic_tooltip)
				return "template_label";
			break;
		case template_project::template_type::free_background:
			if(!c.dynamic_text && !c.dynamic_tooltip)
				return "template_bg_graphic";
			break;
		case template_project::template_type::free_icon:
			if(!c.dynamic_text && !c.dynamic_tooltip)
				return "template_icon_graphic";
			break;
		default:
			break;
	}
	return project_name + "_" + win.wrapped.name + "_" + c.name + "_t";
}

std::string element_initialize_child(std::string const& project_name, window_element_wrapper_t const& win, ui_element_t const& c, open_project_t const& proj) {
	std::string result;

	result += "\t" "\t" "\t" + c.name + " = std::make_unique<" + element_class_name(project_name, win, c) + ">();\n";
	result += "\t" "\t" "\t" + c.name + "->parent = this;\n";
	result += "\t" "\t" "\t" "auto cptr = " + c.name + ".get();\n";

	result += "\t" "\t" "\t" "cptr->base_data.position.x = child_data.x_pos;\n";
	result += "\t" "\t" "\t" "cptr->base_data.position.y = child_data.y_pos;\n";
	result += "\t" "\t" "\t" "cptr->base_data.size.x = child_data.x_size;\n";
	result += "\t" "\t" "\t" "cptr->base_data.size.y = child_data.y_size;\n";

	if(c.updates_while_hidden) {
		result += "\t"  "\t"  "\t" "cptr->flags |= ui::element_base::wants_update_when_hidden_mask;\n";
	}

	switch(c.ttype) {
		case template_project::template_type::label:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_text = state.lookup_key(child_data.text_key);\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::button:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_text = state.lookup_key(child_data.text_key);\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::edit_control:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
		} break;
		case template_project::template_type::toggle_button:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_text = state.lookup_key(child_data.text_key);\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::iconic_button:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "cptr->icon = child_data.icon_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::mixed_button:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "cptr->icon_id = child_data.icon_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
			result += "\t" "\t" "\t"  "if(child_data.text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_text = state.lookup_key(child_data.text_key);\n";
		} break;
		case template_project::template_type::mixed_button_ci:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "cptr->icon_id = child_data.icon_id;\n";
			result += "\t" "\t" "\t"  "cptr->icon_color = child_data.table_divider_color;\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
			result += "\t" "\t" "\t"  "if(child_data.text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_text = state.lookup_key(child_data.text_key);\n";
		} break;
		case template_project::template_type::free_icon:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "cptr->color = child_data.table_divider_color;\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::free_background:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "if(child_data.tooltip_text_key.length() > 0)\n";
			result += "\t" "\t" "\t" "\t" "cptr->default_tooltip = state.lookup_key(child_data.tooltip_text_key);\n";
		} break;
		case template_project::template_type::table_header:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
		} break;
		case template_project::template_type::table_row:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
		} break;
		case template_project::template_type::table_highlights:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
		} break;
		case template_project::template_type::stacked_bar_chart:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
		} break;
		case template_project::template_type::drop_down_control:
		{
			result += "\t" "\t" "\t"  "cptr->template_id = child_data.template_id;\n";
			result += "\t" "\t" "\t"  "cptr->two_columns = (child_data.text_type != aui_text_type::body);\n";
			result += "\t" "\t" "\t"  "cptr->target_page_height = child_data.border_size;\n";
			auto cwindow_type = window_from_name(proj, c.child_window);
			if(cwindow_type) {
				result += "\t" "\t" "\t"  "cptr->label_window_internal = std::unique_ptr<" + project_name + "_" + c.child_window + "_t>(static_cast<" + project_name + "_" + c.child_window + "_t*>(make_" + project_name + "_" + cwindow_type->wrapped.name + "(state).release()));\n";
				result += "\t" "\t" "\t"  "cptr->element_x_size = cptr->label_window_internal->base_data.size.x;\n";
				result += "\t" "\t" "\t"  "cptr->element_y_size = cptr->label_window_internal->base_data.size.y;\n";
				result += "\t" "\t" "\t"  "cptr->label_window = cptr->label_window_internal.get();\n";
			}
		} break;
		default:
		{
			if(c.background == background_type::existing_gfx) {
				result += "\t" "\t" "\t"  "cptr->gfx_key = child_data.texture;\n";
			} else if(background_type_is_textured(c.background)) {
				result += "\t" "\t" "\t"  "cptr->texture_key = child_data.texture;\n";
				if(background_type_requires_border_width(c.background)) {
					result += "\t" "\t" "\t"  "cptr->border_size = child_data.border_size;\n";
				}
				if(c.has_alternate_bg) {
					result += "\t" "\t" "\t"  "cptr->alt_texture_key = child_data.alt_texture;\n";
				}
			}
			if(c.text_key.length() > 0) {
				result += "\t" "\t" "\t"  "cptr->text_key = state.lookup_key(child_data.text_key);\n";
			}
			if(c.text_key.length() > 0 || c.dynamic_text) {
				result += "\t" "\t" "\t"  "cptr->text_scale = child_data.text_scale;\n";
				result += "\t" "\t" "\t"  "cptr->text_is_header = (child_data.text_type == aui_text_type::header);\n";
				result += "\t" "\t" "\t"  "cptr->text_alignment = child_data.text_alignment;\n";
				result += "\t" "\t" "\t"  "cptr->text_color = child_data.text_color;\n";
			}
			if(c.tooltip_text_key.length() > 0) {
				result += "\t" "\t" "\t"  "cptr->tooltip_key = state.lookup_key(child_data.tooltip_text_key);\n";
			}
			if(c.background == background_type::linechart) {
				result += "\t" "\t" "\t"  "cptr->line_color = child_data.table_highlight_color;\n";
			}
			if(c.background == background_type::colorsquare) {
				result += "\t" "\t" "\t"  "cptr->color = child_data.table_highlight_color;\n";
			}
		} break;
	}

	result += "\t" "\t" "\t" "cptr->parent = this;\n";
	result += "\t" "\t" "\t" "cptr->on_create(state);\n";
	result += "\t" "\t" "\t" "children.push_back(cptr);\n";
	result += "\t" "\t" "\t" "pending_children.pop_back(); continue;\n";

	return result;
}

std::string element_type_declarations(std::string const& project_name, window_element_wrapper_t const& win, ui_element_t const& c, code_snippets& old_code, open_project_t const& proj) {
	std::string result;
	if(!element_needs_class(c)) {
		return result;
	}

	std::string base_type = "ui::element_base";
	if(c.ttype != template_project::template_type::none) {
		switch(c.ttype) {
			case template_project::template_type::label:
				base_type = "alice_ui::template_label";
				break;
			case template_project::template_type::free_icon:
				base_type = "alice_ui::template_icon_graphic";
				break;
			case template_project::template_type::free_background:
				base_type = "alice_ui::template_bg_graphic";
				break;
			case template_project::template_type::iconic_button:
				base_type = "alice_ui::template_icon_button";
				break;
			case template_project::template_type::button:
				base_type = "alice_ui::template_text_button";
				break;
			case template_project::template_type::edit_control:
				base_type = "ui::edit_box_element_base";
				break;
			case template_project::template_type::mixed_button:
				base_type = "alice_ui::template_mixed_button";
				break;
			case template_project::template_type::mixed_button_ci:
				base_type = "alice_ui::template_mixed_button_ci";
				break;
			case template_project::template_type::toggle_button:
				base_type = "alice_ui::template_toggle_button";
				break;
			case template_project::template_type::drop_down_control:
				base_type = "alice_ui::template_drop_down_control";
				break;
			default:
				break;
		}
	}

	result += "struct " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t : public " + base_type + " {\n";

	result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::variables\n";
	if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::variables"); it != old_code.found_code.end()) {
		it->second.used = true;
		result += it->second.text;
	}
	result += "// END\n";

	for(auto& dm : c.members) {
		result += "\t" + dm.type + " " + dm.name + ";\n";
	}

	if(c.ttype != template_project::template_type::none) {
		switch(c.ttype) {
			case template_project::template_type::label:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
				}
			} break;
			case template_project::template_type::free_background:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
				}
			} break;
			case template_project::template_type::free_icon:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
				}
			} break;
			case template_project::template_type::button:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.left_click_action) {
					result += "\t"  "bool button_action(sys::state& state) noexcept override;\n";
				}
				if(c.right_click_action) {
					result += "\t"  "bool button_right_action(sys::state& state) noexcept override;\n";
				}
				if(c.shift_click_action) {
					result += "\t"  "bool button_shift_action(sys::state& state) noexcept override;\n";
				}
				if(c.hotkey.size() > 0) {
					result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.hover_activation) {
					result += "\t"  "void button_on_hover(sys::state& state) noexcept override;\n";
					result += "\t"  "void button_on_hover_end(sys::state& state) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::edit_control:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "void on_edit_command(sys::state& state, ui::edit_command command, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.dynamic_text) {
					result += "\t" "void edit_box_update(sys::state& state, std::u16string_view s) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::toggle_button:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.left_click_action) {
					result += "\t"  "bool button_action(sys::state& state) noexcept override;\n";
				}
				if(c.right_click_action) {
					result += "\t"  "bool button_right_action(sys::state& state) noexcept override;\n";
				}
				if(c.shift_click_action) {
					result += "\t"  "bool button_shift_action(sys::state& state) noexcept override;\n";
				}
				if(c.hotkey.size() > 0) {
					result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.hover_activation) {
					result += "\t"  "void button_on_hover(sys::state& state) noexcept override;\n";
					result += "\t"  "void button_on_hover_end(sys::state& state) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::iconic_button:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.left_click_action) {
					result += "\t"  "bool button_action(sys::state& state) noexcept override;\n";
				}
				if(c.right_click_action) {
					result += "\t"  "bool button_right_action(sys::state& state) noexcept override;\n";
				}
				if(c.shift_click_action) {
					result += "\t"  "bool button_shift_action(sys::state& state) noexcept override;\n";
				}
				if(c.hotkey.size() > 0) {
					result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.hover_activation) {
					result += "\t"  "void button_on_hover(sys::state& state) noexcept override;\n";
					result += "\t"  "void button_on_hover_end(sys::state& state) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::mixed_button:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.left_click_action) {
					result += "\t"  "bool button_action(sys::state& state) noexcept override;\n";
				}
				if(c.right_click_action) {
					result += "\t"  "bool button_right_action(sys::state& state) noexcept override;\n";
				}
				if(c.shift_click_action) {
					result += "\t"  "bool button_shift_action(sys::state& state) noexcept override;\n";
				}
				if(c.hotkey.size() > 0) {
					result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.hover_activation) {
					result += "\t"  "void button_on_hover(sys::state& state) noexcept override;\n";
					result += "\t"  "void button_on_hover_end(sys::state& state) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::mixed_button_ci:
			{
				if(c.dynamic_tooltip) {
					result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
					result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
					result += "\t" "}\n";
					result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				}
				if(c.left_click_action) {
					result += "\t"  "bool button_action(sys::state& state) noexcept override;\n";
				}
				if(c.right_click_action) {
					result += "\t"  "bool button_right_action(sys::state& state) noexcept override;\n";
				}
				if(c.shift_click_action) {
					result += "\t"  "bool button_shift_action(sys::state& state) noexcept override;\n";
				}
				if(c.hotkey.size() > 0) {
					result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
				}
				if(c.hover_activation) {
					result += "\t"  "void button_on_hover(sys::state& state) noexcept override;\n";
					result += "\t"  "void button_on_hover_end(sys::state& state) noexcept override;\n";
				}
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::table_row: 
			{
				result += "\t" "int32_t template_id = -1;\n";
				auto t = table_from_name(proj, c.table_connection);
				if(t) {
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t"  "text::layout " + col.internal_data.column_name + "_internal_layout;\n";
							result += "\t"  "int32_t  " + col.internal_data.column_name + "_text_color = " + std::to_string(int32_t(col.display_data.cell_text_color)) + ";\n";
							result += "\t"  "std::string " + col.internal_data.column_name + "_cached_text;\n";

							if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
								result += "\t" "float " + col.internal_data.column_name + "_decimal_pos = 0.0f;";
							}
							result += "\t"  "void set_" + col.internal_data.column_name + "_text(sys::state & state, std::string const& new_text);\n";
						}
					}
				}

				result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";

				result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
				result += "\t" "\t" "return ui::tooltip_behavior::tooltip;\n";
				result += "\t" "}\n";


				result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
				result += "\t" "\t" "if(type == ui::mouse_probe_type::click || type == ui::mouse_probe_type::tooltip) {\n";
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
				result += "\t" "\t" "} else  {\n";
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";

				result += "\t"  "void tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept override;\n";
				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
				result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
				result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
				result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::table_highlights:
			{
				result += "\t" "int32_t template_id = -1;\n";
				result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";

				result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
				result += "\t" "\t" "return ui::message_result::unseen;\n";
				result += "\t" "}\n";

				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::table_header:
			{
				result += "\t" "int32_t template_id = -1;\n";
				auto t = table_from_name(proj, c.table_connection);
				if(t) {
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text && col.display_data.header_key.size() > 0) {
							result += "\t"  "text::layout " + col.internal_data.column_name + "_internal_layout;\n";
							result += "\t"  "std::string " + col.internal_data.column_name + "_cached_text;\n";
						}
					}
				}

				result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";

				result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
				result += "\t" "\t" "return ui::tooltip_behavior::tooltip;\n";
				result += "\t" "}\n";

				result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
				result += "\t" "\t" "if(type == ui::mouse_probe_type::click || type == ui::mouse_probe_type::tooltip) {\n";
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
				result += "\t" "\t" "} else  {\n";
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";

				result += "\t"  "void tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept override;\n";
				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
				result += "\t"  "void on_reset_text(sys::state & state) noexcept override;\n";
				result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
				result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
				result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::stacked_bar_chart:
			{
				result += "\t" "int32_t template_id = -1;\n";
				result += "\t" "ogl::data_texture data_texture{ " + std::to_string(c.datapoints) + ", 3 };\n";
				result += "\t" "struct graph_entry {" + c.list_content + " key; ogl::color3f color; float amount; };\n";
				result += "\t" "std::vector<graph_entry> graph_content;\n";
				result += "\t" "void update_chart(sys::state& state);\n";
				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
				result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";

				result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
				result += "\t" "\t" "return ui::tooltip_behavior::position_sensitive_tooltip;\n";
				result += "\t" "}\n";

				result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
				result += "\t" "\t" "if(type == ui::mouse_probe_type::click) {\n";
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
				result += "\t" "\t" "} else if(type == ui::mouse_probe_type::tooltip) {\n";
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
				result += "\t" "\t" "} else {\n";
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";


				result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			case template_project::template_type::drop_down_control:
			{

				auto cwindow_type = window_from_name(proj, c.child_window);
				std::string insert_params;
				if(cwindow_type) {
					result += "\t" "struct " + c.child_window + "_option { ";
					for(auto& dm : cwindow_type->wrapped.members) {
						insert_params += " " + dm.type + " " + dm.name + ", ";
						result += dm.type + " " + dm.name + "; ";
					}
					result += "};\n";
				}
				if(insert_params.size() >= 2) {
					insert_params.pop_back();
					insert_params.pop_back();
				}

				result += "\t" "std::vector<" + c.child_window + "_option> list_contents;\n";
				result += "\t" "std::vector<std::unique_ptr<" + project_name + "_" + c.child_window + "_t >> list_pool;\n";
				result += "\t" "std::unique_ptr<" + project_name + "_" + c.child_window + "_t> label_window_internal;\n";

				result += "\t" "void add_item(" + insert_params + ");\n";
				
				result += "\t" "ui::element_base* get_nth_item(sys::state& state, int32_t id, int32_t pool_id) override;\n";
				result += "\t" "void quiet_on_selection(sys::state& state, int32_t id);\n";
				result += "\t" "void on_selection(sys::state& state, int32_t id) override;\n";
				result += "\t" "void clear_list();\n";
				
				result += "\t" "void on_create(sys::state& state) noexcept override;\n";
				result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			} break;
			default: break;
		}
	} else {
		if(c.background == background_type::existing_gfx) {
			result += "\t"  "std::string_view gfx_key;\n";
			result += "\t"  "dcon::gfx_object_id background_gid;\n";
			result += "\t"  "int32_t frame = 0;\n";
		} else if(c.background == background_type::progress_bar) {
			result += "\t"  "std::string_view texture_key;\n";
			result += "\t"  "dcon::texture_id background_texture;\n";
			result += "\t"  "std::string_view alt_texture_key;\n";
			result += "\t"  "dcon::texture_id alt_background_texture;\n";
			result += "\t"  "float progress = 0.0f;\n";
		} else if(background_type_is_textured(c.background)) {
			result += "\t"  "std::string_view texture_key;\n";
			if(c.has_alternate_bg) {
				result += "\t"  "std::string_view alt_texture_key;\n";
				result += "\t"  "dcon::texture_id alt_background_texture;\n";
				result += "\t"  "bool is_active = false;\n";
			}
			result += "\t"  "dcon::texture_id background_texture;\n";
			if(background_type_requires_border_width(c.background)) {
				result += "\t"  "int16_t border_size = 0;\n";
			}
			if(background_type_has_frames(c.background)) {
				result += "\t"  "int32_t frame = 0;\n";
			}
		} else if(c.background == background_type::linechart) {
			result += "\t" "ogl::lines lines{ " + std::to_string(c.datapoints) + " };\n";
			result += "\t" "ogl::color4f line_color{ " + std::to_string(c.other_color.r) + "f, " + std::to_string(c.other_color.g) + "f, " + std::to_string(c.other_color.b) + "f, " + std::to_string(c.other_color.a) + "f };\n";
			result += "\t" "void set_data_points(sys::state& state, std::vector<float> const& datapoints, float min, float max);\n";
		} else if(c.background == background_type::doughnut) {
			result += "\t" "ogl::generic_ui_mesh_triangle_strip mesh{ " + std::to_string(c.datapoints * 2) + " };\n";
			result += "\t" "ogl::data_texture data_texture{ " + std::to_string(c.datapoints) + ", 3 };\n";
			result += "\t" "struct graph_entry {" + c.list_content + " key; ogl::color3f color; float amount; };\n";
			result += "\t" "std::vector<graph_entry> graph_content;\n";
			result += "\t" "void update_chart(sys::state& state);\n";
		} else if(c.background == background_type::colorsquare) {
			result += "\t" "ogl::color4f color{ " + std::to_string(c.other_color.r) + "f, " + std::to_string(c.other_color.g) + "f, " + std::to_string(c.other_color.b) + "f, " + std::to_string(c.other_color.a) + "f };\n";
		} else if(c.background == background_type::stackedbarchart) {
			result += "\t" "ogl::data_texture data_texture{ " + std::to_string(c.datapoints) + ", 3 };\n";
			result += "\t" "struct graph_entry {" + c.list_content + " key; ogl::color3f color; float amount; };\n";
			result += "\t" "std::vector<graph_entry> graph_content;\n";
			result += "\t" "void update_chart(sys::state& state);\n";
		} else if(c.background == background_type::flag) {
			result += "\t" "std::variant<std::monostate, dcon::national_identity_id, dcon::rebel_faction_id, dcon::nation_id> flag;\n";
		} else if(c.background == background_type::table_columns) {
			auto t = table_from_name(proj, c.table_connection);
			if(t) {
				for(auto& col : t->table_columns) {
					if(col.internal_data.cell_type == table_cell_type::text) {
						result += "\t"  "text::layout " + col.internal_data.column_name + "_internal_layout;\n";
						result += "\t"  "text::text_color  " + col.internal_data.column_name + "_text_color = text::text_color::" + color_to_name(col.display_data.cell_text_color) + ";\n";
						result += "\t"  "std::string " + col.internal_data.column_name + "_cached_text;\n";

						if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
							result += "\t" "float " + col.internal_data.column_name + "_decimal_pos = 0.0f;";
						}
						result += "\t"  "void set_" + col.internal_data.column_name + "_text(sys::state & state, std::string const& new_text);\n";
					}
				}
			}
		} else if(c.background == background_type::table_headers) {
			auto t = table_from_name(proj, c.table_connection);
			if(t) {
				for(auto& col : t->table_columns) {
					if(col.internal_data.cell_type == table_cell_type::text && col.display_data.header_key.size() > 0) {
						result += "\t"  "text::layout " + col.internal_data.column_name + "_internal_layout;\n";
						result += "\t"  "std::string " + col.internal_data.column_name + "_cached_text;\n";
					}
				}
			}
		}

		if(c.can_disable) {
			result += "\t"  "bool disabled = false;\n";
		}
		if(c.text_key.length() > 0 || c.dynamic_text) {
			result += "\t"  "text::layout internal_layout;\n";
			result += "\t"  "text::text_color text_color = text::text_color::" + color_to_name(c.text_color) + ";\n";
			result += "\t"  "float text_scale = " + std::to_string(c.text_scale) + "f; \n";
			result += "\t"  "bool text_is_header = " + std::string(c.text_type == text_type::header ? "true" : "false") + "; \n";
			result += "\t" "text::alignment text_alignment = text::alignment::" + alignment_to_name(c.text_align) + ";\n";
			result += "\t"  "std::string cached_text;\n";
			if(c.text_key.length() > 0) {
				result += "\t"  "dcon::text_key text_key;\n";
			}

			result += "\t"  "void set_text(sys::state & state, std::string const& new_text);\n";
			result += "\t"  "void on_reset_text(sys::state & state) noexcept override;\n";
		}
		if(c.tooltip_text_key.length() > 0) {
			result += "\t"  "dcon::text_key tooltip_key;\n";
		}
		result += "\t" "void on_create(sys::state& state) noexcept override;\n";

		if(c.text_key.length() > 0 || c.dynamic_text || c.background != background_type::none) {
			result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";
		}

		result += "\t" "ui::tooltip_behavior has_tooltip(sys::state & state) noexcept override {\n";
		if(c.dynamic_tooltip) {
			if(c.background != background_type::stackedbarchart && c.background != background_type::doughnut)
				result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
			else
				result += "\t" "\t" "return ui::tooltip_behavior::position_sensitive_tooltip;\n";
		} else if(c.background == background_type::flag) {
			result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
		} else if(c.tooltip_text_key.length() > 0 || c.background == background_type::table_headers || c.background == background_type::table_columns) {
			result += "\t" "\t" "return ui::tooltip_behavior::tooltip;\n";
		} else {
			result += "\t" "\t" "return ui::tooltip_behavior::no_tooltip;\n";
		}
		result += "\t" "}\n";

		result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
		result += "\t" "\t" "if(type == ui::mouse_probe_type::click) {\n";
		if(c.background == background_type::flag || c.background == background_type::table_headers || c.background == background_type::table_columns || c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) {
			result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
		} else {
			result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
		}
		result += "\t" "\t" "} else if(type == ui::mouse_probe_type::tooltip) {\n";
		if(c.dynamic_tooltip || c.tooltip_text_key.length() > 0 || c.background == background_type::flag || c.background == background_type::table_headers || c.background == background_type::table_columns) {
			if(c.background == background_type::doughnut) {
				result += "\t" "\t" "\t" "auto cx = (float)(x - base_data.size.x / 2) / (float)(base_data.size.x);\n";
				result += "\t" "\t" "\t" "auto cy = (float)(y - base_data.size.y / 2) / (float)(base_data.size.y);\n";
				result += "\t" "\t" "\t" "auto d = std::sqrt(cx * cx + cy * cy);\n";
				result += "\t" "\t" "\t" "if (d > 0.3f && d < 0.5f) return ui::message_result::consumed;\n";
				result += "\t" "\t" "\t" "else return ui::message_result::unseen;\n";
			} else {
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
			}
		} else {
			result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
		}
		result += "\t" "\t" "} else if(type == ui::mouse_probe_type::scroll) {\n";
		result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
		result += "\t" "\t" "} else {\n";
		result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
		result += "\t" "\t" "}\n";
		result += "\t" "}\n";

		if(c.background == background_type::table_columns || c.background == background_type::table_headers) {
			result += "\t"  "void tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept override;\n";
			if(c.background == background_type::table_headers)
				result += "\t"  "void on_reset_text(sys::state & state) noexcept override;\n";
		}

		if(c.background != background_type::none || c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) {
			result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
			result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
			if(c.hotkey.size() > 0) {
				result += "\t" "ui::message_result on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept override;\n";
			}
		}
		if(c.hover_activation) {
			result += "\t"  "void on_hover(sys::state& state) noexcept override;\n";
			result += "\t"  "void on_hover_end(sys::state& state) noexcept override;\n";
		}
		if(c.dynamic_tooltip || c.tooltip_text_key.length() > 0 || c.background == background_type::flag || c.background == background_type::table_columns || c.background == background_type::table_headers) {
			result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
		}
		result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
	}

	if(!c.members.empty()) {
		result += "\t" "void* get_by_name(sys::state& state, std::string_view name_parameter) noexcept override {\n";
		for(auto& m : c.members) {
			result += "\t" "\t" "if(name_parameter == \"" + m.name + "\") {\n";
			result += "\t" "\t" "\t" "return (void*)(&" + m.name + ");\n";
			result += "\t" "\t" "}\n";
		}
		result += "\t" "\t" "return nullptr;\n";
		result += "\t" "}\n";
	}

	result += "};\n";

	return result;
}

std::string element_member_functions(std::string const& project_name, window_element_wrapper_t const& win, ui_element_t const& c, code_snippets& old_code, open_project_t const& proj) {
	std::string result;

	if(!element_needs_class(c)) {
		return result;
	}

	auto make_parent_var_text = [&](bool skip_self = false) {
		window_element_t const* w = &win.wrapped;
		std::string parent_string = "parent";
		while(w != nullptr) {
			if(skip_self) {

			} else {
				result += "\t" + project_name + "_" + w->name + "_t& " + w->name + " = *((" + project_name + "_" + w->name + "_t*)(" + parent_string + ")); \n";
			}
			if(w->parent.size() > 0) {
				bool found = false;
				bool from_layout = w->parent_is_layout;
				for(auto& wwin : proj.windows) {
					if(wwin.wrapped.name == w->parent) {
						w = &wwin.wrapped;
						found = true;
						break;
					}
				}
				if(!found)
					w = nullptr;
				if(skip_self || from_layout) {
					skip_self = false;
					parent_string += "->parent";
				} else {
					parent_string += "->parent->parent";
				}

			} else {
				w = nullptr;
			}
		}
	};

	if(c.ttype != template_project::template_type::none) {
		switch(c.ttype) {
			case template_project::template_type::label:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					//UPDATE
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
			} break;
			case template_project::template_type::free_background:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					//UPDATE
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
			} break;
			case template_project::template_type::free_icon:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
				if(c.dynamic_text || c.dynamic_tooltip) {
					//UPDATE
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
			} break;
			case template_project::template_type::edit_control:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_edit_command(sys::state& state, ui::edit_command command, sys::key_modifiers mods)  noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::edit_command\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::edit_command"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "ui::edit_box_element_base::on_edit_command(state, command, mods);\n";
					result += "}\n";
				}
				if(c.dynamic_text) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::edit_box_update(sys::state& state, std::u16string_view s) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::edit_update\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::edit_update"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";


				//ON CREATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";
			} break;
			case template_project::template_type::button:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}
				
				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";
				
				if(c.left_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.right_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_right_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.shift_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_shift_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.hotkey.size() > 0) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "if(key == sys::virtual_key::" + c.hotkey + " && !disabled) {\n";
					result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
					result += "\t" "\t" "return ui::message_result::consumed;\n";
					result += "\t" "}\n";
					result += "\t" "return ui::message_result::unseen;\n";
					result += "}\n";
				}
				if(c.hover_activation) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover_end(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

			} break;
			case template_project::template_type::toggle_button:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				if(c.left_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.right_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_right_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.shift_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_shift_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.hotkey.size() > 0) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "if(key == sys::virtual_key::" + c.hotkey + " && !disabled) {\n";
					result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
					result += "\t" "\t" "return ui::message_result::consumed;\n";
					result += "\t" "}\n";
					result += "\t" "return ui::message_result::unseen;\n";
					result += "}\n";
				}
				if(c.hover_activation) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover_end(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

			} break;
			case template_project::template_type::iconic_button:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				if(c.left_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.right_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_right_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result +="\t" "return true;\n";
					result +="}\n";
				}
				if(c.shift_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_shift_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.hotkey.size() > 0) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "if(key == sys::virtual_key::" + c.hotkey + " && !disabled) {\n";
					result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
					result += "\t" "\t" "return ui::message_result::consumed;\n";
					result += "\t" "}\n";
					result += "\t" "return ui::message_result::unseen;\n";
					result += "}\n";
				}
				if(c.hover_activation) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover_end(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

			} break;
			case template_project::template_type::mixed_button:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				if(c.left_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.right_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_right_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.shift_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_shift_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result +="\t" "return true;\n";
					result += "}\n";
				}
				if(c.hotkey.size() > 0) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "if(key == sys::virtual_key::" + c.hotkey + " && !disabled) {\n";
					result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
					result += "\t" "\t" "return ui::message_result::consumed;\n";
					result += "\t" "}\n";
					result += "\t" "return ui::message_result::unseen;\n";
					result += "}\n";
				}
				if(c.hover_activation) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover_end(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

			} break;
			case template_project::template_type::mixed_button_ci:
			{
				if(c.dynamic_tooltip) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				if(c.left_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.right_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_right_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.shift_click_action) {
					result += "bool " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_shift_action(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "\t" "return true;\n";
					result += "}\n";
				}
				if(c.hotkey.size() > 0) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "if(key == sys::virtual_key::" + c.hotkey + " && !disabled) {\n";
					result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
					result += "\t" "\t" "return ui::message_result::consumed;\n";
					result += "\t" "}\n";
					result += "\t" "return ui::message_result::unseen;\n";
					result += "}\n";
				}
				if(c.hover_activation) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::button_on_hover_end(sys::state& state) noexcept {\n";
					make_parent_var_text();
					result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
					if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
						it->second.used = true;
						result += it->second.text;
					}
					result += "// END\n";
					result += "}\n";
				}

			} break;
			case template_project::template_type::drop_down_control:
			{
				auto cwindow_type = window_from_name(proj, c.child_window);
				std::string insert_params;
				std::string insert_vals;
				if(cwindow_type) {
					for(auto& dm : cwindow_type->wrapped.members) {
						insert_params += " " + dm.type + " " + dm.name + ", ";
						insert_vals += dm.name + ", ";
					}
				}
				if(insert_params.size() >= 2) {
					insert_params.pop_back();
					insert_params.pop_back();
				}
				if(insert_vals.size() >= 2) {
					insert_vals.pop_back();
					insert_vals.pop_back();
				}
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::add_item(" + insert_params + ") {\n";
				result += "\t" "list_contents.emplace_back(" + c.child_window + "_option{" + insert_vals + "});\n";
				result += "\t" "++total_items;\n";
				result += "}\n";

				result += "ui::element_base* " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::get_nth_item(sys::state& state, int32_t id, int32_t pool_id) {\n";
				result += "\t" "while(pool_id >= int32_t(list_pool.size())) {\n";
				result += "\t" "\t" "list_pool.emplace_back(static_cast<" + project_name + "_" + c.child_window + "_t*>(make_" + project_name + "_" + cwindow_type->wrapped.name + "(state).release()));\n";
				result += "\t" "}\n";
				if(cwindow_type) {
					for(auto& dm : cwindow_type->wrapped.members) {
						result += "\t" "list_pool[pool_id]->" + dm.name + " = list_contents[id]." + dm.name + "; \n";
					}
				}
				result += "\t" "return list_pool[pool_id].get();\n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::quiet_on_selection(sys::state& state, int32_t id) {\n";
				result += "\t" "if(id < 0 || id >= int32_t(list_contents.size())) return;\n";
				result += "\t" "selected_item = id;\n";
				if(cwindow_type) {
					for(auto& dm : cwindow_type->wrapped.members) {
						result += "\t" "label_window_internal->" + dm.name + " = list_contents[id]." + dm.name + "; \n";
					}
				}
				result += "\t" "label_window_internal->impl_on_update(state); \n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_selection(sys::state& state, int32_t id) {\n";
				result += "\t" "quiet_on_selection(state, id);\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_selection\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_selection"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::clear_list() {";
				result += "\t" "list_contents.clear();\n";
				result += "\t" "total_items = 0;\n";
				result += "}\n";

				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";


				//ON CREATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "\t" "template_drop_down_control::on_create(state);\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";
			} break;
			case template_project::template_type::table_row:
			{
				result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
				result += "\t" "return ui::message_result::consumed;\n";
				result += "}\n";

				result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
				result += "\t" "return ui::message_result::consumed;\n";
				result += "}\n";

				auto t = table_from_name(proj, c.table_connection);

				if(t) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept {\n";

					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					int32_t ccountb = 0;
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "if(x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x < table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
							if(col.internal_data.has_dy_cell_tooltip || col.display_data.cell_tooltip_key.length() > 0) {
								result += "\t" "\t" "ident = " + std::to_string(ccountb) + ";\n";
								result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
								result += "\t" "\t" "subrect.top_left.x += int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start);\n";
								result += "\t" "\t" "subrect.size = base_data.size;\n";
								result += "\t" "\t" "subrect.size.x = int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
								result += "\t" "\t" "return;\n";
							}
							result += "\t" "}\n";
							++ccountb;
						}
					}
					result += "\t" "\t" "ident = -1;\n";
					result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
					result += "\t" "\t" "subrect.size = base_data.size;\n";
					result += "}\n";

					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "if(x >=  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x <  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start +  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
							
							if(col.internal_data.has_dy_cell_tooltip) {
								make_parent_var_text();
								result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::column_tooltip\n";
								if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::column_tooltip"); it != old_code.found_code.end()) {
									it->second.used = true;
									result += it->second.text;
								}
								result += "// END\n";

							} else if(col.display_data.cell_tooltip_key.length() > 0) {
								result += "\t" "text::add_line(state, contents, table_source->" + t->name + "_" + col.internal_data.column_name + "_column_tooltip_key);\n";
							}
							result += "\t" "}\n";
						}
					}
					result += "}\n";
				}

				if(t) {
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::set_" + col.internal_data.column_name + "_text(sys::state & state, std::string const& new_text) {\n";
							result += "\t" "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
							result += "\t" "if(new_text !=  " + col.internal_data.column_name + "_cached_text) {\n";
							result += "\t" "\t" + col.internal_data.column_name + "_cached_text = new_text;\n";
							result += "\t" "\t" + col.internal_data.column_name + "_internal_layout.contents.clear();\n";
							result += "\t" "\t" + col.internal_data.column_name + "_internal_layout.number_of_lines = 0;\n";
							result += "\t" "\t" "{\n";
							result += "\t" "\t" "text::single_line_layout sl{ " + col.internal_data.column_name + "_internal_layout, text::layout_parameters{ 0, 0, int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - " + std::to_string(2 * proj.grid_size) + "), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, false, 1.0f * " + std::to_string(2 * proj.grid_size) + "), 0, table_source->" + t->name + "_" + col.internal_data.column_name + "_text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr }; \n";
							result += "\t" "\t" "sl.add_text(state, " + col.internal_data.column_name + "_cached_text);\n";
							result += "\t" "\t" "}\n";

							if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
								result += "\t" "float temp_decimal_pos = -1.0f;\n";
								result += "\t" "float running_total = 0.0f;\n";
								result += "\t" "auto best_cluster = std::string::npos;\n";
								result += "\t" "auto found_decimal_pos =  " + col.internal_data.column_name + "_cached_text.find_last_of('.');";
								result += "\t" "bool left_align = " + std::string(col.internal_data.decimal_alignment == aui_text_alignment::right ? "true" : "false") + " == (state_is_rtl(state)); \n";
								result += "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) { \n";
								result += "\t" "\t" "running_total = float(t.x);\n";
								result += "\t" "\t" "for(auto& ch : t.unicodechars.glyph_info) {\n";
								result += "\t" "\t" "\t" "if(found_decimal_pos <= size_t(ch.cluster) && size_t(ch.cluster) < best_cluster) {\n";
								result += "\t" "\t" "\t" "\t" "temp_decimal_pos = running_total ;\n";
								result += "\t" "\t" "\t" "\t" "best_cluster = size_t(ch.cluster);\n";
								result += "\t" "\t" "\t" "}\n";
								result += "\t" "\t" "\t" "running_total += ch.x_advance / (state.user_settings.ui_scale * text::fixed_to_fp);\n";
								result += "\t" "\t" "}\n";
								result += "\t" "} \n";
								result += "\t" "if(best_cluster == std::string::npos) {\n";
								result += "\t" "\t" "running_total = 0.0f;\n";
								result += "\t" "\t" "temp_decimal_pos = -1000000.0f;\n";
								result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
								result += "\t" "\t" "\t" "running_total = float(t.x);\n";
								result += "\t" "\t" "\t" "for(auto& ch : t.unicodechars.glyph_info) {\n";
								result += "\t" "\t" "\t" "\t" "temp_decimal_pos = std::max(temp_decimal_pos, running_total);\n";
								result += "\t" "\t" "\t" "\t" "running_total += ch.x_advance / (state.user_settings.ui_scale * text::fixed_to_fp)\n";
								result += "\t" "\t" "\t" "}\n";
								result += "\t" "\t" "}\n";
								result += "\t" "}\n";
								result += "\t" + col.internal_data.column_name + "_decimal_pos = temp_decimal_pos;\n";
								result += "\t" "if(left_align)\n";
								result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::min(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
								result += "\t" "else\n";
								result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::max(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
							}
							result += "\t" "} else {\n";
							if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
								result += "\t" "bool left_align = " + std::string(col.internal_data.decimal_alignment == aui_text_alignment::right ? "true" : "false") + " == (state_is_rtl(state)); \n";
								result += "\t" "if(left_align)\n";
								result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::min(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
								result += "\t" "else\n";
								result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::max(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
							}
							result += "\t" "}\n";

							result += "}\n";
						}
					}
				}

				
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				if(t) {
					result += "\t" "auto fh = text::make_font_id(state, false, 1.0f * " + std::to_string(proj.grid_size * 2) + ");\n";
					result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
					result += "\t" "auto ycentered = (base_data.size.y - linesz) / 2;\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					result += "\t" "auto abs_location = ui::get_absolute_location(state, *this);\n";
					result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - abs_location.x;\n";
					result += "\t" "int32_t rel_mouse_y = int32_t(state.mouse_y_position / state.user_settings.ui_scale) - abs_location.y;\n";
					result += "\t" "auto ink_color =template_id != -1 ? ogl::color3f(state.ui_templates.colors[state.ui_templates.table_t[template_id].table_color]) : ogl::color3f{}; ";

					for(auto& col : t->table_columns) {
						result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";

						result += "\t" "if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y + base_data.size.y - 2), float(2), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(1), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.25f), float(y + base_data.size.y - 1), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.5f), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(!(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y) && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell above/below
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && !col_um_" + col.internal_data.column_name + "){\n"; // case over another cell in line
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result +="\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t}\n";

						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "auto col_color_" + col.internal_data.column_name + " = state.ui_templates.colors[" + col.internal_data.column_name + "_text_color]; ";
							result += "\t"  "if(!" + col.internal_data.column_name + "_internal_layout.contents.empty() && linesz > 0.0f) {\n";

							result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
							if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
								result += "\t" "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(proj.grid_size) + " + table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos - " + col.internal_data.column_name + "_decimal_pos, float(y + int32_t(ycentered)),  fh, ogl::color3f{ col_color_" + col.internal_data.column_name + ".r, col_color_" + col.internal_data.column_name + ".g, col_color_" + col.internal_data.column_name + ".b }, ogl::color_modification::none);\n";
							} else {
								result += "\t" "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(proj.grid_size) + ", float(y + int32_t(ycentered)),  fh, ogl::color3f{ col_color_" + col.internal_data.column_name + ".r, col_color_" + col.internal_data.column_name + ".g, col_color_" + col.internal_data.column_name + ".b }, ogl::color_modification::none);\n";
							}
							result += "\t" "\t" "}\n";
							result += "\t"  "}\n";
						}
					}

				}
				result += "}\n";
			
				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";


				//ON CREATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

			} break;
			case template_project::template_type::table_highlights:
			{
				auto t = table_from_name(proj, c.table_connection);

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				if(t) {					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					result += "\t" "auto abs_location = ui::get_absolute_location(state, *this);\n";
					result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - abs_location.x;\n";
					result += "\t" "int32_t rel_mouse_y = int32_t(state.mouse_y_position / state.user_settings.ui_scale) - abs_location.y;\n";
					result += "\t" "auto ink_color =template_id != -1 ? ogl::color3f(state.ui_templates.colors[state.ui_templates.table_t[template_id].table_color]) : ogl::color3f{}; ";

					for(auto& col : t->table_columns) {
						result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";

						result += "\t" "if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y + base_data.size.y - 2), float(2), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(1), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.25f), float(y + base_data.size.y - 1), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.5f), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(!(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y) && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell above/below
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && !col_um_" + col.internal_data.column_name + "){\n"; // case over another cell in line
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t}\n";
					}
				}
				result += "}\n";


				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "}\n";

			} break;
			case template_project::template_type::table_header:
			{
				auto t = table_from_name(proj, c.table_connection);

				if(t) {
					result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					for(auto& col : t->table_columns) {
						if(col.internal_data.sortable && col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "if(x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x < table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
							result += "\t" "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
							result += "\t" "\t" "auto old_direction = table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction;\n";
							for(auto& colb : t->table_columns) {
								if(colb.internal_data.sortable) {
									result += "\t" "\t" "table_source->" + t->name + "_" + colb.internal_data.column_name + "_sort_direction = 0;\n";
								}
							}
							result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction = int8_t(old_direction <= 0 ? 1 : -1);\n";
							result += "\t" "\t" "parent->parent->impl_on_update(state);\n";
							result += "\t" "}\n";
						}
					}
					result += "\t" "return ui::message_result::consumed;";
					result += "}\n";
				}
				result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
				result += "\t" "return ui::message_result::consumed;\n";
				result += "}\n";

				if(t) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept {\n";

					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					int32_t ccountb = 0;
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "if(x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x < table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
							if(col.internal_data.has_dy_header_tooltip || col.display_data.header_tooltip_key.length() > 0) {
								result += "\t" "\t" "ident = " + std::to_string(ccountb) + ";\n";
								result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
								result += "\t" "\t" "subrect.top_left.x += int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start);\n";
								result += "\t" "\t" "subrect.size = base_data.size;\n";
								result += "\t" "\t" "subrect.size.x = int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
								result += "\t" "\t" "return;\n";
							}
							result += "\t" "}\n";
							++ccountb;
						}
					}
					result += "\t" "\t" "ident = -1;\n";
					result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
					result += "\t" "\t" "subrect.size = base_data.size;\n";
					result += "}\n";

					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "if(x >=  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x <  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start +  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
							
							if(col.internal_data.has_dy_header_tooltip) {
								make_parent_var_text();
								result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::header_tooltip\n";
								if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::header_tooltip"); it != old_code.found_code.end()) {
									it->second.used = true;
									result += it->second.text;
								}
								result += "// END\n";

							} else if(col.display_data.header_tooltip_key.length() > 0) {
								result += "\t" "text::add_line(state, contents, table_source->" + t->name + "_" + col.internal_data.column_name + "_header_tooltip_key);\n";
							}
							result += "\t" "}\n";
						}
					}
					result += "}\n";
				}

				if(t) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_reset_text(sys::state& state) noexcept {\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					for(auto& col : t->table_columns) {
						if(col.internal_data.cell_type == table_cell_type::text && col.display_data.header_key.size() > 0) {
							result += "\t" "{\n";
							result += "\t" "" + col.internal_data.column_name + "_cached_text = text::produce_simple_string(state, table_source->" + t->name + "_" + col.internal_data.column_name + "_header_text_key);\n";
							result += "\t" " " + col.internal_data.column_name + "_internal_layout.contents.clear();\n";
							result += "\t" " " + col.internal_data.column_name + "_internal_layout.number_of_lines = 0;\n";
							result += "\t" "text::single_line_layout sl{  " + col.internal_data.column_name + "_internal_layout, text::layout_parameters{ 0, 0, int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width" + (col.internal_data.sortable ? " - " + std::to_string(proj.grid_size * 0) : std::string("")) + " - " + std::to_string(2 * proj.grid_size) + "), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, false, 1.0f * " + std::to_string(2 * proj.grid_size) + "), 0, table_source->" + t->name + "_" + col.internal_data.column_name + "_text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
							result += "\t" "sl.add_text(state, " + col.internal_data.column_name + "_cached_text);\n";
							result += "\t" "}\n";
						}
					}
					result += "}\n";
				}

				
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				if(t) {
					result += "\t" "auto fh = text::make_font_id(state, false, 1.0f * " + std::to_string(proj.grid_size * 2) + ");\n";
					result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
					result += "\t" "auto ycentered = (base_data.size.y - linesz) / 2;\n";
					result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					result += "\t" "auto abs_location = ui::get_absolute_location(state, *this);\n";
					result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - abs_location.x;\n";
					result += "\t" "int32_t rel_mouse_y = int32_t(state.mouse_y_position / state.user_settings.ui_scale) - abs_location.y;\n";
					result += "\t" "auto ink_color =template_id != -1 ? ogl::color3f(state.ui_templates.colors[state.ui_templates.table_t[template_id].table_color]) : ogl::color3f{}; ";

					for(auto& col : t->table_columns) {
						result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";

						if(col.internal_data.cell_type == table_cell_type::text) {
							if(col.internal_data.sortable) {
								result += "\t" "\t" "{\n";
								result += "\t" "\t" "auto bg = template_id != -1 ? ((0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && col_um_" + col.internal_data.column_name + ") ? state.ui_templates.table_t[template_id].active_header_bg : state.ui_templates.table_t[template_id].interactable_header_bg) : -1;\n";
								result += "\t" "\t" "if(bg != -1)\n";
								result += "\t" "\t" "ogl::render_textured_rect_direct(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(base_data.size.y), "
									"state.ui_templates.backgrounds[bg].renders.get_render(state, float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) / float(table_source->grid_size), float(base_data.size.y) / float(table_source->grid_size), int32_t(table_source->grid_size), state.user_settings.ui_scale)"
									"); \n";
								result += "\t" "\t" "}\n";
							}
						}

						result += "\t" "if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y + base_data.size.y - 2), float(2), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(1), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.25f), float(y + base_data.size.y - 1), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width * 0.5f), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(!(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y) && col_um_" + col.internal_data.column_name + "){\n"; // case over this cell above/below
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(1), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - 2), float(y), float(2), float(base_data.size.y), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";

						result += "\t" "} else if(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y && !col_um_" + col.internal_data.column_name + "){\n"; // case over another cell in line
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y + base_data.size.y - 2), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(2), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
						result += "\t}\n";

						if(col.internal_data.cell_type == table_cell_type::text) {
							result += "\t" "auto col_color_" + col.internal_data.column_name + " = state.ui_templates.colors[table_source->" + t->name + "_" + col.internal_data.column_name + "_header_text_color]; ";

							if(col.internal_data.sortable) {
								result += "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction > 0) {\n";
								result += "\t" "\t" "auto icon = template_id != -1 ? state.ui_templates.table_t[template_id].arrow_increasing : -1;\n";
								result += "\t" "\t" "if(icon != -1)\n";
								result += "\t" "\t" "ogl::render_textured_rect_direct(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(0) + "), float(y + base_data.size.y / 2 - " + std::to_string(proj.grid_size) + "), float(" + std::to_string(proj.grid_size * 1) + "), float(" + std::to_string(proj.grid_size * 2) + "), "
									"state.ui_templates.icons[icon].renders.get_render(state, " + std::to_string(proj.grid_size * 1) + ", " + std::to_string(proj.grid_size * 2) + ", state.user_settings.ui_scale, ink_color.r, ink_color.g, ink_color.b)"
									"); \n";
								result += "\t" "}\n";
							
								result += "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction < 0) {\n";
								result += "\t" "\t" "auto icon = template_id != -1 ? state.ui_templates.table_t[template_id].arrow_decreasing : -1;\n";
								result += "\t" "\t" "if(icon != -1)\n";
								result += "\t" "\t" "ogl::render_textured_rect_direct(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(0) + "), float(y + base_data.size.y / 2 - " + std::to_string(proj.grid_size) + "), float(" + std::to_string(proj.grid_size * 1) + "), float(" + std::to_string(proj.grid_size * 2) + "), "
									"state.ui_templates.icons[icon].renders.get_render(state, " + std::to_string(proj.grid_size * 1) + ", " + std::to_string(proj.grid_size * 2) + ", state.user_settings.ui_scale, ink_color.r, ink_color.g, ink_color.b)"
								"); \n";
								result += "\t" "}\n";
							}
							

							if(col.display_data.header_key.size() > 0) {
								result += "\t"  "if(!" + col.internal_data.column_name + "_internal_layout.contents.empty() && linesz > 0.0f) {\n";

								result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
								result += "\t" "\t" "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start" + (col.internal_data.sortable ? " + " + std::to_string(proj.grid_size * 0) : std::string("")) + " + " + std::to_string(proj.grid_size) + ", float(y + int32_t(ycentered)),  fh, ogl::color3f{ col_color_" + col.internal_data.column_name + ".r, col_color_" + col.internal_data.column_name + ".g, col_color_" + col.internal_data.column_name + ".b }, ogl::color_modification::none);\n";
								result += "\t" "\t" "}\n";
								result += "\t"  "}\n";
							}
						}
					}

					result += "\t" "if(!(0 <= rel_mouse_y && rel_mouse_y < base_data.size.y)){\n";
					result += "\t" "ogl::render_alpha_colored_rect(state, float(x), float(y + base_data.size.y - 1), float(base_data.size.x), float(1), ink_color.r, ink_color.g, ink_color.b, 1.0f);\n";
					result += "\t" "}\n";
				}
				result += "}\n";
			
				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				//ON CREATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";
			} break;
			case template_project::template_type::stacked_bar_chart:
			{
				//ON CREATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_chart(sys::state& state) {\n";
				if(c.has_alternate_bg == false)
					result += "\t" "std::sort(graph_content.begin(), graph_content.end(), [](auto const& a, auto const& b) { return a.amount > b.amount; });\n";
				result += "\t" "float total = 0.0f;\n";
				result += "\t" "for(auto& e : graph_content) { total += e.amount; }\n";
				result += "\t" "if(total <= 0.0f) {\n";
				result += "\t" "\t" "for(int32_t k = 0; k < " + std::to_string(c.datapoints) + "; k++) {\n";
				result += "\t" "\t" "\t" "data_texture.data[k * 3] = uint8_t(0);\n";
				result += "\t" "\t" "\t""data_texture.data[k * 3 + 1] = uint8_t(0);\n";
				result += "\t" "\t" "\t" "data_texture.data[k * 3 + 2] = uint8_t(0);\n";
				result += "\t" "\t" "}\n";
				result += "\t" "\t" "data_texture.data_updated = true;\n";
				result += "\t" "\t" "return;\n";
				result += "\t" "}\n";
				result += "\t" "int32_t index = 0;\n";
				result += "\t" "float offset = 0.0f;\n";
				result += "\t" "for(int32_t k = 0; k < " + std::to_string(c.datapoints) + "; k++) {\n";
				result += "\t" "\t" "while(graph_content[index].amount + offset < (float(k) + 0.5f) * total /  float(" + std::to_string(c.datapoints) + ")) {\n";
				result += "\t" "\t" "\t" "offset += graph_content[index].amount;\n";
				result += "\t" "\t" "\t" "++index;\n";
				result += "\t" "\t" "}\n";
				result += "\t" "\t" "data_texture.data[k * 3] = uint8_t(graph_content[index].color.r * 255.0f);\n";
				result += "\t" "\t" "data_texture.data[k * 3 + 1] = uint8_t(graph_content[index].color.g * 255.0f);\n";
				result += "\t" "\t""data_texture.data[k * 3 + 2] = uint8_t(graph_content[index].color.b * 255.0f);\n";
				result += "\t" "}\n";
				result += "\t" "data_texture.data_updated = true;\n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
				make_parent_var_text();

				result += "\t" "if(template_id == -1) return;\n";
				result += "\t" "alice_ui::layout_window_element* par = static_cast<alice_ui::layout_window_element*>(parent);\n";
				result += "\t" "float temp_total = 0.0f;\n";
				result += "\t" "for(auto& p : graph_content) { temp_total += p.amount; }\n";
				result += "\t" "float temp_offset = temp_total * float(x - par->grid_size * state.ui_templates.stacked_bar_t[template_id].l_margin ) / float(base_data.size.x - par->grid_size *(state.ui_templates.stacked_bar_t[template_id].l_margin +state.ui_templates.stacked_bar_t[template_id].r_margin));\n";
				result += "\t" "int32_t temp_index = 0;\n";
				result += "\t" "if(temp_offset < 0.0f || temp_offset > temp_total) return;\n";
				result += "\t" "for(auto& p : graph_content) { if(temp_offset <= p.amount) break; temp_offset -= p.amount; ++temp_index; }\n";
				result += "\t" "if(temp_index < int32_t(graph_content.size())) {\n";
				result += "\t" "\t" "auto& selected_key = graph_content[temp_index].key;\n";
				
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				if(c.background == background_type::stackedbarchart || c.background == background_type::doughnut) {
					result += "\t" "}\n";
				}
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				result += "\t" "if(template_id == -1) return;\n";
				result += "\t" "alice_ui::layout_window_element* par = static_cast<alice_ui::layout_window_element*>(parent);\n";
				result += "\t" "ogl::render_stripchart(state, ogl::color_modification::none, float(x + par->grid_size * state.ui_templates.stacked_bar_t[template_id].l_margin), float(y + par->grid_size * state.ui_templates.stacked_bar_t[template_id].t_margin), float(base_data.size.x - par->grid_size *(state.ui_templates.stacked_bar_t[template_id].l_margin +state.ui_templates.stacked_bar_t[template_id].r_margin)), float(base_data.size.y - par->grid_size *(state.ui_templates.stacked_bar_t[template_id].t_margin +state.ui_templates.stacked_bar_t[template_id].b_margin)), data_texture);\n";

				result += "\t" "auto bg_id = state.ui_templates.stacked_bar_t[template_id].overlay_bg;\n";
				result += "\t" "if(bg_id != -1)\n";
				result += "\t" "\t" "ogl::render_textured_rect_direct(state, float(x), float(y), float(base_data.size.x), float(base_data.size.y), state.ui_templates.backgrounds[bg_id].renders.get_render(state, float(base_data.size.x) / float(par->grid_size), float(base_data.size.y) / float(par->grid_size), int32_t(par->grid_size), state.user_settings.ui_scale));\n";
				
				result += "}\n";


				//UPDATE
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
				make_parent_var_text();
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "}\n";

			} break;
			default: break;
		}

		return result;
	}

	//HOVER
	if(c.hover_activation) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_hover(sys::state& state) noexcept {\n";
		make_parent_var_text();
		result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";
		result += "}\n";
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_hover_end(sys::state& state) noexcept {\n";
		make_parent_var_text();
		result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::on_hover_end\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::on_hover_end"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";
		result += "}\n";
	}
	// MOUSE
	if(c.background == background_type::table_headers) {
		auto t = table_from_name(proj, c.table_connection);
		if(t) {
			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
			result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
			for(auto& col : t->table_columns) {
				if(col.internal_data.sortable && col.internal_data.cell_type == table_cell_type::text) {
					result += "\t" "if(x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x < table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
					result += "\t" "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
					result += "\t" "\t" "auto old_direction = table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction;\n";
					for(auto& colb : t->table_columns) {
						if(colb.internal_data.sortable) {
							result += "\t" "\t" "table_source->" + t->name + "_" + colb.internal_data.column_name + "_sort_direction = 0;\n";
						}
					}
					result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction = int8_t(old_direction <= 0 ? 1 : -1);\n";
					result += "\t" "\t" "parent->parent->impl_on_update(state);\n";
					result += "\t" "}\n";
				}
			}
			result += "\t" "return ui::message_result::consumed;";
			result += "}\n";
		}
		result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
		result += "\t" "return ui::message_result::consumed;\n";
		result += "}\n";
	} else if(c.background != background_type::none || c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) {
		result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
		if(c.left_click_action || c.shift_click_action) {
			if(c.can_disable) {
				result += "\t" "if(disabled) return ui::message_result::consumed;\n";
			}
			make_parent_var_text();
			result += "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
			if(c.shift_click_action) {
				result += "\t" "if(mods == sys::key_modifiers::modifiers_shift) {\n";
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_shift_action\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_shift_action"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
				result += "\t" "\t" "return ui::message_result::consumed;\n";
				result += "\t" "}\n";
			}
			if(c.left_click_action) {
				result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::lbutton_action\n";
				if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::lbutton_action"); it != old_code.found_code.end()) {
					it->second.used = true;
					result += it->second.text;
				}
				result += "// END\n";
			}
		}
		if(c.left_click_action || c.shift_click_action || c.right_click_action) {
			result += "\t" "return ui::message_result::consumed;\n";
		} else if(c.background == background_type::flag) {
			if(c.can_disable) {
				result += "\t" "if(disabled) return ui::message_result::consumed;\n";
			}
			result += "\t" "if(std::holds_alternative<dcon::nation_id>(flag)) {\n";
			result += "\t" "\t" "if(std::get<dcon::nation_id>(flag) && state.world.nation_get_owned_province_count(std::get<dcon::nation_id>(flag)) > 0) {\n";
			result += "\t" "\t" "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
			result += "\t" "\t" "\t" "state.open_diplomacy(std::get<dcon::nation_id>(flag));\n";
			result += "\t" "\t" "} \n";
			result += "\t" "} else if(std::holds_alternative<dcon::national_identity_id>(flag)) {\n";
			result += "\t" "\t" "auto n_temp = state.world.national_identity_get_nation_from_identity_holder(std::get<dcon::national_identity_id>(flag));\n";
			result += "\t" "\t" "if(n_temp && state.world.nation_get_owned_province_count(n_temp) > 0) {\n";
			result += "\t" "\t" "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
			result += "\t" "\t" "\t" "state.open_diplomacy(n_temp);\n";
			result += "\t" "\t" "} \n";
			result += "\t" "} \n";
			result += "\t" "return ui::message_result::consumed;\n";
		} else {
			result += "\t" "return ui::message_result::unseen;\n";
		}
		result += "}\n";

		result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
		if(c.right_click_action) {
			if(c.can_disable) {
				result += "\t" "if(disabled) return ui::message_result::consumed;\n";
			}
			make_parent_var_text();
			result += "\t" "sound::play_interface_sound(state, sound::get_click_sound(state), state.user_settings.interface_volume* state.user_settings.master_volume);\n";
			result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::rbutton_action\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::rbutton_action"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";
		}
		if(c.left_click_action || c.shift_click_action || c.right_click_action) {
			result += "\t" "return ui::message_result::consumed;\n";
		} else {
			result += "\t" "return ui::message_result::unseen;\n";
		}
		result += "}\n";

		if(c.hotkey.size() > 0) {
			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_key_down(sys::state& state, sys::virtual_key key, sys::key_modifiers mods) noexcept {\n";
			result += "\t" "if(key == sys::virtual_key::" + c.hotkey + std::string(c.can_disable ? " && !disabled" : "") + ") {\n";
			result += "\t" "\t" "on_lbutton_down(state, 0, 0, mods);\n";
			result += "\t" "\t" "return ui::message_result::consumed;\n";
			result += "\t" "}\n";
			result += "\t" "return ui::message_result::unseen;\n";
			result += "}\n";
		}
	}

	// SPECIAL
	if(c.background == background_type::linechart) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::set_data_points(sys::state& state, std::vector<float> const& datapoints, float min, float max) {\n";

		result += "\t" "assert(datapoints.size() ==  " + std::to_string(c.datapoints) + ");\n";
		result += "\t" "float y_height = max - min;\n";
		result += "\t" "std::vector<float> scaled_datapoints = std::vector<float>(" + std::to_string(c.datapoints) + ");\n";
		result += "\t" "if(y_height == 0.f) {\n";
		result += "\t" "\t" "for(size_t i = 0; i < " + std::to_string(c.datapoints) + "; i++) {\n";
		result += "\t" "\t" "\t" "scaled_datapoints[i] = .5f;\n";
		result += "\t" "\t" "}\n";
		result += "\t" "} else {\n";
		result += "\t" "\t" "for(size_t i = 0; i < " + std::to_string(c.datapoints) + "; i++) {\n";
		result += "\t" "\t" "\t" "scaled_datapoints[i] = (datapoints[i] - min) / y_height;\n";
		result += "\t" "\t" "}\n";
		result += "\t" "}\n";
		result += "\t" "lines.set_y(scaled_datapoints.data());\n";

		result += "}\n";
	} else if(c.background == background_type::stackedbarchart || c.background == background_type::doughnut) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_chart(sys::state& state) {\n";

		if(c.has_alternate_bg == false)
			result += "\t" "std::sort(graph_content.begin(), graph_content.end(), [](auto const& a, auto const& b) { return a.amount > b.amount; });\n";
		result += "\t" "float total = 0.0f;\n";
		result += "\t" "for(auto& e : graph_content) { total += e.amount; }\n";
		result += "\t" "if(total <= 0.0f) {\n";
		result += "\t" "\t" "for(int32_t k = 0; k < " + std::to_string(c.datapoints) + "; k++) {\n";
		result += "\t" "\t" "\t" "data_texture.data[k * 3] = uint8_t(0);\n";
		result += "\t" "\t" "\t""data_texture.data[k * 3 + 1] = uint8_t(0);\n";
		result += "\t" "\t" "\t" "data_texture.data[k * 3 + 2] = uint8_t(0);\n";
		result += "\t" "\t" "}\n";
		result += "\t" "\t" "data_texture.data_updated = true;\n";
		result += "\t" "\t" "return;\n";
		result += "\t" "}\n";
		result += "\t" "int32_t index = 0;\n";
		result += "\t" "float offset = 0.0f;\n";
		result += "\t" "for(int32_t k = 0; k < " + std::to_string(c.datapoints) + "; k++) {\n";
		result += "\t" "\t" "while(graph_content[index].amount + offset < (float(k) + 0.5f) * total /  float(" + std::to_string(c.datapoints) + ")) {\n";
		result += "\t" "\t" "\t" "offset += graph_content[index].amount;\n";
		result += "\t" "\t" "\t" "++index;\n";
		result += "\t" "\t" "}\n";
		result += "\t" "\t" "data_texture.data[k * 3] = uint8_t(graph_content[index].color.r * 255.0f);\n";
		result += "\t" "\t" "data_texture.data[k * 3 + 1] = uint8_t(graph_content[index].color.g * 255.0f);\n";
		result += "\t" "\t""data_texture.data[k * 3 + 2] = uint8_t(graph_content[index].color.b * 255.0f);\n";
		result += "\t" "}\n";
		result += "\t" "data_texture.data_updated = true;\n";

		result += "}\n";
	}

	// TOOLTIP
	if(c.background == background_type::table_columns || c.background == background_type::table_headers) {
		auto t = table_from_name(proj, c.table_connection);
		if(t) {
			result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::tooltip_position(sys::state& state, int32_t x, int32_t y, int32_t& ident, ui::urect& subrect) noexcept {\n";

			result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
			int32_t ccountb = 0;
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					result += "\t" "if(x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x < table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
					if(col.internal_data.has_dy_header_tooltip || col.display_data.header_tooltip_key.length() > 0) {
						result += "\t" "\t" "ident = " + std::to_string(ccountb) + ";\n";
						result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
						result += "\t" "\t" "subrect.top_left.x += int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start);\n";
						result += "\t" "\t" "subrect.size = base_data.size;\n";
						result += "\t" "\t" "subrect.size.x = int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
						result += "\t" "\t" "return;\n";
					}
					result += "\t" "}\n";
					++ccountb;
				}
			}
			result += "\t" "\t" "ident = -1;\n";
			result += "\t" "\t" "subrect.top_left = ui::get_absolute_location(state, *this);\n";
			result += "\t" "\t" "subrect.size = base_data.size;\n";
			result += "}\n";

			result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
			result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					result += "\t" "if(x >=  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && x <  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start +  table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width) {\n";
					if(c.background == background_type::table_headers) {
						if(col.internal_data.has_dy_header_tooltip) {
							make_parent_var_text();
							result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::header_tooltip\n";
							if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::header_tooltip"); it != old_code.found_code.end()) {
								it->second.used = true;
								result += it->second.text;
							}
							result += "// END\n";

						} else if(col.display_data.header_tooltip_key.length() > 0) {
							result += "\t" "text::add_line(state, contents, table_source->" + t->name + "_" + col.internal_data.column_name + "_header_tooltip_key);\n";
						}
					} else if(c.background == background_type::table_columns) {
						if(col.internal_data.has_dy_cell_tooltip) {
							make_parent_var_text();
							result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::column_tooltip\n";
							if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::" + col.internal_data.column_name + "::column_tooltip"); it != old_code.found_code.end()) {
								it->second.used = true;
								result += it->second.text;
							}
							result += "// END\n";

						} else if(col.display_data.cell_tooltip_key.length() > 0) {
							result += "\t" "text::add_line(state, contents, table_source->" + t->name + "_" + col.internal_data.column_name + "_column_tooltip_key);\n";
						}
					}
					result += "\t" "}\n";
				}
			}
			result += "}\n";
		}
	} else if(c.dynamic_tooltip) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
		make_parent_var_text();
		if(c.background == background_type::stackedbarchart) {
			result += "\t" "float temp_total = 0.0f;\n";
			result += "\t" "for(auto& p : graph_content) { temp_total += p.amount; }\n";
			result += "\t" "float temp_offset = temp_total * float(x) / float(base_data.size.x);\n";
			result += "\t" "int32_t temp_index = 0;\n";
			result += "\t" "for(auto& p : graph_content) { if(temp_offset <= p.amount) break; temp_offset -= p.amount; ++temp_index; }\n";
			result += "\t" "if(temp_index < int32_t(graph_content.size())) {\n";
			result += "\t" "\t" "auto& selected_key = graph_content[temp_index].key;\n";
		}
		if(c.background == background_type::doughnut) {
			result += "\t" "float temp_total = 0.0f;\n";
			result += "\t" "for(auto& p : graph_content) { temp_total += p.amount; }\n";
			result += "\t" "float x_normal = float(x) / float(base_data.size.x) * 2.f - 1.f;\n";
			result += "\t" "float y_normal = float(y) / float(base_data.size.y) * 2.f - 1.f;\n";
			result += "\t" "float temp_offset = temp_total * (std::atan2(-y_normal, -x_normal) / std::numbers::pi_v<float> / 2.f + 0.5f);\n";
			result += "\t" "int32_t temp_index = 0;\n";
			result += "\t" "for(auto& p : graph_content) { if(temp_offset <= p.amount) break; temp_offset -= p.amount; ++temp_index; }\n";
			result += "\t" "if(temp_index < int32_t(graph_content.size())) {\n";
			result += "\t" "\t" "auto& selected_key = graph_content[temp_index].key;\n";
		}
		result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::tooltip\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::tooltip"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";
		if(c.background == background_type::stackedbarchart || c.background == background_type::doughnut) {
			result += "\t" "}\n";
		}
		result += "}\n";
	} else if(c.tooltip_text_key.length() > 0) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
		result += "\t" "text::add_line(state, contents, tooltip_key);\n";
		result += "}\n";
	} else if(c.background == background_type::flag) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
		result += "\t" "if(std::holds_alternative<dcon::nation_id>(flag)) {\n";
		result += "\t" "\t" "text::add_line(state, contents, text::get_name(state, std::get<dcon::nation_id>(flag)));\n";
		result += "\t" "} else if(std::holds_alternative<dcon::national_identity_id>(flag)) {\n";
		result += "\t" "\t" "text::add_line(state, contents, nations::name_from_tag(state, std::get<dcon::national_identity_id>(flag)));\n";
		result += "\t" "} else if(std::holds_alternative<dcon::rebel_faction_id>(flag)) {\n";
		result += "\t" "\t" "auto box = text::open_layout_box(contents, 0);\n";
		result += "\t" "\t" "text::add_to_layout_box(state, contents, box, rebel::rebel_name(state, std::get<dcon::rebel_faction_id>(flag)));\n";
		result += "\t" "\t" "text::close_layout_box(contents, box);\n";
		result += "\t" "} \n";
		result += "}\n";
	}

	// TEXT
	if(c.text_key.length() > 0 || c.dynamic_text) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::set_text(sys::state& state, std::string const& new_text) {\n";
		result += "\t" "if(new_text != cached_text) {\n";
		result += "\t" "\t" "cached_text = new_text;\n";
		result += "\t" "\t" "internal_layout.contents.clear();\n";
		result += "\t" "\t" "internal_layout.number_of_lines = 0;\n";
		result += "\t" "\t" "text::single_line_layout sl{ internal_layout, text::layout_parameters{ 0, 0, static_cast<int16_t>(base_data.size.x), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, text_is_header, text_scale * " + std::to_string(2 * proj.grid_size) + "), 0, text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
		result += "\t" "\t" "sl.add_text(state, cached_text);\n";
		result += "\t" "}\n";
		result += "}\n";

		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_reset_text(sys::state& state) noexcept {\n";
		if(!c.dynamic_text) {
			result += "\t" "cached_text = text::produce_simple_string(state, text_key);\n";
			result += "\t" "internal_layout.contents.clear();\n";
			result += "\t" "internal_layout.number_of_lines = 0;\n";
			result += "\t" "text::single_line_layout sl{ internal_layout, text::layout_parameters{ 0, 0, static_cast<int16_t>(base_data.size.x), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, text_is_header, text_scale * " + std::to_string(2 * proj.grid_size) + "), 0, text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
			result += "\t" "sl.add_text(state, cached_text);\n";
		}
		result += "}\n";
	}
	if(c.background == background_type::table_headers) {
		auto t = table_from_name(proj, c.table_connection);
		if(t) {
			result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_reset_text(sys::state& state) noexcept {\n";
			result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text && col.display_data.header_key.size() > 0) {
					result += "\t" "{\n";
					result += "\t" "" + col.internal_data.column_name + "_cached_text = text::produce_simple_string(state, table_source->" + t->name + "_" + col.internal_data.column_name + "_header_text_key);\n";
					result += "\t" " " + col.internal_data.column_name + "_internal_layout.contents.clear();\n";
					result += "\t" " " + col.internal_data.column_name + "_internal_layout.number_of_lines = 0;\n";
					result += "\t" "text::single_line_layout sl{  " + col.internal_data.column_name + "_internal_layout, text::layout_parameters{ 0, 0, int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width" + (col.internal_data.sortable ? " - " + std::to_string(proj.grid_size * 0) : std::string("")) + " - " + std::to_string(2 * proj.grid_size) + "), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, false, 1.0f * " + std::to_string(2 * proj.grid_size) + "), 0, table_source->" + t->name + "_" + col.internal_data.column_name + "_text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
					result += "\t" "sl.add_text(state, " + col.internal_data.column_name + "_cached_text);\n";
					result += "\t" "}\n";
				}
			}
			result += "}\n";
		}
	}

	if(c.background == background_type::table_columns) {
		auto t = table_from_name(proj, c.table_connection);
		if(t) {
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::set_" + col.internal_data.column_name + "_text(sys::state & state, std::string const& new_text) {\n";
					result += "\t" "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
					result += "\t" "if(new_text !=  " + col.internal_data.column_name + "_cached_text) {\n";
					result += "\t" "\t" + col.internal_data.column_name + "_cached_text = new_text;\n";
					result += "\t" "\t" + col.internal_data.column_name + "_internal_layout.contents.clear();\n";
					result += "\t" "\t" + col.internal_data.column_name + "_internal_layout.number_of_lines = 0;\n";
					result += "\t" "\t" "{\n";
					result += "\t" "\t" "text::single_line_layout sl{ " + col.internal_data.column_name + "_internal_layout, text::layout_parameters{ 0, 0, int16_t(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width - " + std::to_string(2 * proj.grid_size) + "), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, false, 1.0f * " + std::to_string(2 * proj.grid_size) + "), 0, table_source->" + t->name + "_" + col.internal_data.column_name + "_text_alignment, text::text_color::black, true, true }, state_is_rtl(state) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr }; \n";
					result += "\t" "\t" "sl.add_text(state, " + col.internal_data.column_name + "_cached_text);\n";
					result += "\t" "\t" "}\n";

					if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
						result += "\t" "auto font_size_factor = float(text::size_from_font_id(text::make_font_id(state, false, 1.0f * " + std::to_string(2 * proj.grid_size) + "))) / (float((1 << 6) * 64.0f * text::magnification_factor));\n";
						result += "\t" "float temp_decimal_pos = -1.0f;\n";
						result += "\t" "float running_total = 0.0f;\n";
						result += "\t" "auto best_cluster = std::string::npos;\n";
						result += "\t" "auto found_decimal_pos =  " + col.internal_data.column_name + "_cached_text.find_last_of('.');";
						result += "\t" "bool left_align = " + std::string(col.internal_data.decimal_alignment == aui_text_alignment::right ? "true" : "false") + " == (state_is_rtl(state)); \n";
						result += "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) { \n";
						result += "\t" "\t" "running_total = float(t.x);\n";
						result += "\t" "\t" "for(auto& ch : t.unicodechars.glyph_info) {\n";
						result += "\t" "\t" "\t" "if(found_decimal_pos <= size_t(ch.cluster) && size_t(ch.cluster) < best_cluster) {\n";
						result += "\t" "\t" "\t" "\t" "temp_decimal_pos = running_total ;\n";
						result += "\t" "\t" "\t" "\t" "best_cluster = size_t(ch.cluster);\n";
						result += "\t" "\t" "\t" "}\n";
						result += "\t" "\t" "\t" "running_total += ch.x_advance * font_size_factor;\n";
						result += "\t" "\t" "}\n";
						result += "\t" "} \n";
						result += "\t" "if(best_cluster == std::string::npos) {\n";
						result += "\t" "\t" "running_total = 0.0f;\n";
						result += "\t" "\t" "temp_decimal_pos = -1000000.0f;\n";
						result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
						result += "\t" "\t" "\t" "running_total = float(t.x);\n";
						result += "\t" "\t" "\t" "for(auto& ch : t.unicodechars.glyph_info) {\n";
						result += "\t" "\t" "\t" "\t" "temp_decimal_pos = std::max(temp_decimal_pos, running_total);\n";
						result += "\t" "\t" "\t" "\t" "running_total += ch.x_advance * font_size_factor;\n";
						result += "\t" "\t" "\t" "}\n";
						result += "\t" "\t" "}\n";
						result += "\t" "}\n";
						result += "\t" + col.internal_data.column_name + "_decimal_pos = temp_decimal_pos;\n";
						result += "\t" "if(left_align)\n";
						result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::min(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
						result += "\t" "else\n";
						result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::max(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";


					}
					result += "\t" "} else {\n";
					if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
						result += "\t" "bool left_align = " + std::string(col.internal_data.decimal_alignment == aui_text_alignment::right ? "true" : "false") + " == (state_is_rtl(state)); \n";
						result += "\t" "if(left_align)\n";
						result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::min(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
						result += "\t" "else\n";
						result += "\t" "\t" "table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = std::max(" + col.internal_data.column_name + "_decimal_pos, table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos);\n";
					}
					result += "\t" "}\n";

					result += "}\n";
				}
			}
		}
	}

	// RENDER
	if(c.text_key.length() > 0 || c.dynamic_text || c.background != background_type::none) {
		result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
		if(c.background == background_type::existing_gfx) {
			result += "\t" "if(background_gid) {\n";
			result += "\t""\t" "auto& gfx_def = state.ui_defs.gfx[background_gid];\n";
			result += "\t" "\t" "if(gfx_def.primary_texture_handle) {\n";
			result += "\t"  "\t" "\t" "if(gfx_def.get_object_type() == ui::object_type::bordered_rect) {\n";
			result += "\t" "\t" "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), gfx_def.type_dependent, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			result += "\t""\t" "\t" "} else if(gfx_def.number_of_frames > 1) {\n";
			result += "\t" "\t" "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), frame, gfx_def.number_of_frames, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			result += "\t" "\t" "\t" "} else {\n";
			result += "\t" "\t" "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			result += "\t" "\t" "\t" "}\n";
			result += "\t" "\t" "}\n";
			result += "\t" "}\n";
		} else if(c.background == background_type::icon_strip) {
			if(c.has_alternate_bg) {
				result += "\t"  "if(is_active) { \n";
				result += "\t" "\t" "auto tid = ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key);\n";
				result += "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), frame, state.open_gl.asset_textures[alt_background_texture].size_x / std::max(1, state.open_gl.asset_textures[alt_background_texture].size_y), float(x), float(y), float(base_data.size.x), float(base_data.size.y),  tid, base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				result += "\t"  "} else { \n";
				result += "\t" "\t" "auto tid = ogl::get_late_load_texture_handle(state, background_texture, texture_key);\n";
				result += "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), frame, state.open_gl.asset_textures[background_texture].size_x / std::max(1, state.open_gl.asset_textures[background_texture].size_y), float(x), float(y), float(base_data.size.x), float(base_data.size.y),  tid, base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				result += "\t"  "} \n";
			} else {
				result += "\t" "auto tid = ogl::get_late_load_texture_handle(state, background_texture, texture_key);\n";
				result += "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), frame, state.open_gl.asset_textures[background_texture].size_x / std::max(1, state.open_gl.asset_textures[background_texture].size_y), float(x), float(y), float(base_data.size.x), float(base_data.size.y),  tid, base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			}
		} else if(c.background == background_type::texture) {
			if(c.has_alternate_bg) {
				result += "\t"  "if(is_active)\n";
				result += "\t"   "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				result += "\t"  "else\n";
				result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
			} else {
				result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
			}
		} else if(c.background == background_type::progress_bar) {
			result += "\t"   "\t" "ogl::render_progress_bar(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), progress, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
		} else if(c.background == background_type::bordered_texture) {
			if(c.has_alternate_bg) {
				result += "\t" "if(is_active)\n";
				result += "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				result += "\t" "else\n";
				result += "\t"  "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			} else {
				result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
			}
		} else if(c.background == background_type::border_texture_repeat) {
			result += "\t" "ogl::render_rect_with_repeated_border(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(" + std::to_string(proj.grid_size) + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
		} else if(c.background == background_type::textured_corners) {
			result += "\t" "ogl::render_rect_with_repeated_corner(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "), float(" + std::to_string(proj.grid_size) + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
		} else if(c.background == background_type::linechart) {
			result += "\t" "ogl::render_linegraph(state, ogl::color_modification::none, float(x), float(y), base_data.size.x, base_data.size.y, line_color.r, line_color.g, line_color.b, line_color.a, lines);\n";
		} else if(c.background == background_type::doughnut) {
			result += "\t" "ogl::render_ui_mesh(state, ogl::color_modification::none, float(x), float(y), base_data.size.x, base_data.size.y, mesh, data_texture);\n";
		} else if(c.background == background_type::stackedbarchart) {
			result += "\t" "ogl::render_stripchart(state, ogl::color_modification::none, float(x), float(y), float(base_data.size.x), float(base_data.size.y), data_texture);\n";
		} else if(c.background == background_type::colorsquare) {
			result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x ), float(y), float(base_data.size.x), float(base_data.size.y), color.r, color.g, color.b, color.a);\n";
		} else if(c.background == background_type::flag) {
			result += "\t" "if(std::holds_alternative<dcon::nation_id>(flag)) {\n";
			result += "\t" "\t" "dcon::government_flag_id flag_type = dcon::government_flag_id{};\n";
			result += "\t" "\t" "auto h_temp = state.world.nation_get_identity_from_identity_holder(std::get<dcon::nation_id>(flag));\n";
			result += "\t" "\t" "if(bool(std::get<dcon::nation_id>(flag)) && state.world.nation_get_owned_province_count(std::get<dcon::nation_id>(flag)) != 0) {\n";
			result += "\t"  "\t" "\t" "flag_type = culture::get_current_flag_type(state, std::get<dcon::nation_id>(flag));\n";
			result += "\t" "\t" "} else {\n";
			result += "\t"  "\t" "\t" "flag_type = culture::get_current_flag_type(state, h_temp);\n";
			result += "\t" "\t" "}\n";
			result += "\t" "\t" "auto flag_texture_handle = ogl::get_flag_handle(state, h_temp, flag_type);\n";
			result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", true), float(x), float(y), float(base_data.size.x), float(base_data.size.y), flag_texture_handle, base_data.get_rotation(), false,  false);\n";

			result += "\t" "} else if(std::holds_alternative<dcon::national_identity_id>(flag)) {\n";
			result += "\t" "\t" "dcon::government_flag_id flag_type = dcon::government_flag_id{};\n";
			result += "\t" "\t" "auto n_temp = state.world.national_identity_get_nation_from_identity_holder(std::get<dcon::national_identity_id>(flag));\n";
			result += "\t" "\t" "if(bool(n_temp) && state.world.nation_get_owned_province_count(n_temp) != 0) {\n";
			result += "\t"  "\t" "\t" "flag_type = culture::get_current_flag_type(state, n_temp);\n";
			result += "\t" "\t" "} else {\n";
			result += "\t"  "\t" "\t" "flag_type = culture::get_current_flag_type(state, std::get<dcon::national_identity_id>(flag));\n";
			result += "\t" "\t" "}\n";
			result += "\t" "\t" "auto flag_texture_handle = ogl::get_flag_handle(state, std::get<dcon::national_identity_id>(flag), flag_type);\n";
			result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", true), float(x), float(y), float(base_data.size.x), float(base_data.size.y), flag_texture_handle, base_data.get_rotation(), false,  false);\n";

			result += "\t" "} else if(std::holds_alternative<dcon::rebel_faction_id>(flag)) {\n";
			result += "\t" "\t" "dcon::rebel_faction_id rf_temp = std::get<dcon::rebel_faction_id>(flag);\n";
			result += "\t" "\t" "if(state.world.rebel_faction_get_type(rf_temp).get_independence() != uint8_t(culture::rebel_independence::none) && state.world.rebel_faction_get_defection_target(rf_temp)) {\n";
			result += "\t" "\t" "\t" "dcon::government_flag_id flag_type = dcon::government_flag_id{};\n";
			result += "\t" "\t" "\t" "auto h_temp = state.world.rebel_faction_get_defection_target(rf_temp);\n";
			result += "\t" "\t" "\t" "auto n_temp = state.world.national_identity_get_nation_from_identity_holder(h_temp);\n";
			result += "\t" "\t"  "\t" "if(bool(n_temp) && state.world.nation_get_owned_province_count(n_temp) != 0) {\n";
			result += "\t"  "\t" "\t"  "\t" "flag_type = culture::get_current_flag_type(state, n_temp);\n";
			result += "\t" "\t"  "\t" "} else {\n";
			result += "\t"  "\t"  "\t" "\t" "flag_type = culture::get_current_flag_type(state, h_temp);\n";
			result += "\t" "\t"  "\t" "}\n";
			result += "\t" "\t"  "\t" "auto flag_texture_handle = ogl::get_flag_handle(state, h_temp, flag_type);\n";
			result += "\t" "\t"  "\t" "ogl::render_textured_rect(state, ui::get_color_modification(false, " + std::string(c.can_disable ? "disabled" : "false") + ", false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), flag_texture_handle, base_data.get_rotation(), false,  false);\n";
			result += "\t" "\t"  "\t" "ogl::render_textured_rect(state, ui::get_color_modification(false, " + std::string(c.can_disable ? "disabled" : "false") + ", false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_rebel_flag_overlay(state), base_data.get_rotation(), false,  false);\n";
			result += "\t" "\t" "return;\n";
			result += "\t" "\t" "}\n";
			result += "\t" "\t"  "ogl::render_textured_rect(state, ui::get_color_modification(false, " + std::string(c.can_disable ? "disabled" : "false") + ", false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_rebel_flag_handle(state, rf_temp), base_data.get_rotation(), false,  false);\n";
			result += "\t" "} \n";
		} else if(c.background == background_type::table_columns) {
			auto t = table_from_name(proj, c.table_connection);
			if(t) {
				result += "\t" "auto fh = text::make_font_id(state, false, 1.0f * " + std::to_string(proj.grid_size * 2) + ");\n";
				result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
				result += "\t" "auto ycentered = (base_data.size.y - linesz) / 2;\n";
				result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
				result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - ui::get_absolute_location(state, *this).x;\n";


				for(auto& col : t->table_columns) {
					if(col.internal_data.cell_type == table_cell_type::text) {
						result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";

						result += "\t"  "if(!" + col.internal_data.column_name + "_internal_layout.contents.empty() && linesz > 0.0f) {\n";
						result += "\t"  "\t" "auto cmod = ui::get_color_modification(this == state.ui_state.under_mouse && col_um_" + col.internal_data.column_name + " , false, false); \n";
						result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
						if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
							result += "\t" "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(proj.grid_size) + " + table_source->" + t->name + "_" + col.internal_data.column_name + "_decimal_pos - " + col.internal_data.column_name + "_decimal_pos, float(y + int32_t(ycentered)),  fh, ui::get_text_color(state, " + col.internal_data.column_name + "_text_color), cmod);\n";
						} else {
							result += "\t" "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(proj.grid_size) + ", float(y + int32_t(ycentered)),  fh, ui::get_text_color(state, " + col.internal_data.column_name + "_text_color), cmod);\n";
						}
						result += "\t" "\t" "}\n";
						result += "\t"  "}\n";
					}
				}
			}
		} else if(c.background == background_type::table_headers) {
			auto t = table_from_name(proj, c.table_connection);
			if(t) {
				result += "\t" "auto fh = text::make_font_id(state, false, 1.0f * " + std::to_string(proj.grid_size * 2) + ");\n";
				result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
				result += "\t" "auto ycentered = (base_data.size.y - linesz) / 2;\n";
				result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent->parent);\n";
				result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - ui::get_absolute_location(state, *this).x;\n";

				for(auto& col : t->table_columns) {
					if(col.internal_data.cell_type == table_cell_type::text) {
						result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";

						if(col.internal_data.sortable) {
							if(t->ascending_sort_icon.length() > 0) {
								result += "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction > 0) {\n";
								result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse && col_um_" + col.internal_data.column_name + ", " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string(col.internal_data.sortable ? "true" : "false") + "), float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(0) + "), float(y + base_data.size.y / 2 - " + std::to_string(proj.grid_size) + "), float(" + std::to_string(proj.grid_size * 1) + "), float(" + std::to_string(proj.grid_size * 2) + "), ogl::get_late_load_texture_handle(state, table_source->" + t->name + "_ascending_icon, table_source->" + t->name + "_ascending_icon_key), ui::rotation::upright, false, state_is_rtl(state));\n";
								result += "\t" "}\n";
							}
							if(t->descending_sort_icon.length() > 0) {
								result += "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction < 0) {\n";
								result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse && col_um_" + col.internal_data.column_name + ", " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string(col.internal_data.sortable ? "true" : "false") + "), float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + " + std::to_string(0) + "), float(y + base_data.size.y / 2 - " + std::to_string(proj.grid_size) + "), float(" + std::to_string(proj.grid_size * 1) + "), float(" + std::to_string(proj.grid_size * 2) + "), ogl::get_late_load_texture_handle(state, table_source->" + t->name + "_descending_icon, table_source->" + t->name + "_descending_icon_key), ui::rotation::upright, false, state_is_rtl(state));\n";
								result += "\t" "}\n";
							}
						}

						if(col.display_data.header_key.size() > 0) {
							result += "\t"  "if(!" + col.internal_data.column_name + "_internal_layout.contents.empty() && linesz > 0.0f) {\n";
							result += "\t"  "\t" "auto cmod = ui::get_color_modification(this == state.ui_state.under_mouse && col_um_" + col.internal_data.column_name + " , false, " + std::string(col.internal_data.sortable ? "true" : "false") + "); \n";
							result += "\t" "\t" "for(auto& t : " + col.internal_data.column_name + "_internal_layout.contents) {\n";
							result += "\t" "\t" "\t" "ui::render_text_chunk(state, t, float(x) + t.x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start" + (col.internal_data.sortable ? " + " + std::to_string(proj.grid_size * 0) : std::string("")) + " + " + std::to_string(proj.grid_size) + ", float(y + int32_t(ycentered)),  fh, ui::get_text_color(state, table_source->" + t->name + "_" + col.internal_data.column_name + "_header_text_color), cmod);\n";
							result += "\t" "\t" "}\n";
							result += "\t"  "}\n";
						}
					}
				}

				result += "\t" "ogl::render_alpha_colored_rect(state, float(x), float(y + base_data.size.y - 1), float(base_data.size.x), float(1), table_source->" + t->name + "_divider_color.r, table_source->" + t->name + "_divider_color.g, table_source->" + t->name + "_divider_color.b, 1.0f);\n";
			}
		}

		if(c.text_key.length() > 0 || c.dynamic_text) {
			result += "\t"  "if(internal_layout.contents.empty()) return;\n";
			result += "\t"  "auto fh = text::make_font_id(state, text_is_header, text_scale * " + std::to_string(proj.grid_size * 2) + ");\n";
			result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
			result += "\t" "if(linesz == 0.0f) return;\n";
			result += "\t"  "auto ycentered = (base_data.size.y - linesz) / 2;\n";
			result += "\t"  "auto cmod = ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action || c.hover_activation) ? "true" : "false") + "); \n";
			result += "\t"  "for(auto& t : internal_layout.contents) {\n";
			result += "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x, float(y + int32_t(ycentered)),  fh, ui::get_text_color(state, text_color), cmod);\n";
			result += "\t"  "}\n";
		}
		result += "}\n";
	}


	//UPDATE
	result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_update(sys::state& state) noexcept {\n";
	make_parent_var_text();
	result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::update\n";
	if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::update"); it != old_code.found_code.end()) {
		it->second.used = true;
		result += it->second.text;
	}
	result += "// END\n";
	result += "}\n";

	//ON CREATE
	result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_create(sys::state& state) noexcept {\n";
	if(c.background == background_type::existing_gfx) {
		result += "\t" "if(auto it = state.ui_state.gfx_by_name.find(state.lookup_key(gfx_key)); it != state.ui_state.gfx_by_name.end()) {\n";
		result += "\t" "\t" "background_gid = it->second;\n";
		result += "\t" "}\n";
	}
	if(c.text_key.size() > 0) {
		result += "\t" "on_reset_text(state);\n";
	}
	result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
	if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
		it->second.used = true;
		result += it->second.text;
	}
	result += "// END\n";
	result += "}\n";

	return result;
}

std::string generate_project_code(open_project_t& proj, code_snippets& old_code) {
	// fix parents
	for(auto& win : proj.windows) {
		for(auto& c : win.children) {
			if(c.container_type != container_type::none && c.ttype != template_project::template_type::drop_down_control && c.child_window.size() > 0) {
				for(auto& winb : proj.windows) {
					if(winb.wrapped.name == c.child_window) {
						winb.wrapped.parent = win.wrapped.name;
						winb.wrapped.parent_is_layout = false;
					}
				}
			}
		}
		for(auto& owin : proj.windows) {
			if(owin.wrapped.name != win.wrapped.name && any_layout_contains_window(owin.layout, win.wrapped.name)) {
				win.wrapped.parent = owin.wrapped.name;
				win.wrapped.parent_is_layout = true;
			}
		}
	}
	std::string result;
	result += "namespace alice_ui {\n";

	auto project_name = fs::native_to_utf8(proj.project_name);

	// predeclare
	for(auto& win : proj.windows) {
		for(auto& c : win.children) {
			result += element_type_pre_declarations(project_name, win, c);
		}
		result += "struct " + project_name + "_" + win.wrapped.name + "_t;\n";
	}

	// type declarations
	for(auto& win : proj.windows) {
		for(auto& c : win.children) {
			result += element_type_declarations(project_name, win, c, old_code, proj);
		}

		auto gens = make_generators_list(win.layout);
		for(auto g : gens) {
			result += "struct " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t : public layout_generator {\n";
			result += "// BEGIN " + win.wrapped.name + "::" + g->name + "::variables\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::" + g->name + "::variables"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";

			std::string insert_types;
			std::string insert_params;
			std::vector<std::string> header_list;
			for(auto& inserts : g->inserts) {
				insert_params.clear();
				bool found = false;
				for(auto& window : proj.windows) {
					if(window.wrapped.name == inserts.name) {
						insert_types += ", " + inserts.name + "_option";
						result += "\t" "struct " + inserts.name + "_option { ";
						for(auto& dm : window.wrapped.members) {
							insert_params += " " + dm.type + " " + dm.name + ", ";
							result += dm.type + " " + dm.name + "; ";
						}
						result += "};\n";
						found = true;
					}
				}
				if(found) {
					if(inserts.header.size() > 0 && std::find(header_list.begin(), header_list.end(), inserts.header) == header_list.end()) {
						header_list.push_back(inserts.header);
					}
					result += "\t" "std::vector<std::unique_ptr<ui::element_base>> " + inserts.name + "_pool;\n";
					result += "\t" "int32_t " + inserts.name + "_pool_used = 0;\n";
					if(insert_params.size() >= 2) {
						insert_params.pop_back();
						insert_params.pop_back();
					}
					result += "\t" "void add_" + inserts.name + "(" + insert_params + ");\n";
				}
			}
			for(auto& h : header_list) {
				result += "\t" "std::vector<std::unique_ptr<ui::element_base>> " + h + "_pool;\n";
				result += "\t" "int32_t " + h + "_pool_used = 0;\n";
			}

			result += "\t" "std::vector<std::variant<std::monostate" + insert_types + ">> values;\n";

			result += "\t" "void on_create(sys::state& state, layout_window_element* container);\n";
			result += "\t" "void update(sys::state& state, layout_window_element* container);\n";
			result += "\t" "measure_result place_item(sys::state& state, ui::non_owning_container_base* destination, size_t index, int32_t x, int32_t y, bool first_in_section, bool& alternate) override;\n";
			result += "\t" "size_t item_count() override { return values.size(); };\n";
			result += "\t" "void reset_pools() override;\n";
			result += "};\n";
		}
	}
	for(auto& win : proj.windows) {
		if(win.layout.contents.empty() && win.wrapped.template_id == -1)
			result += "struct " + project_name + "_" + win.wrapped.name + "_t : public ui::non_owning_container_base {\n";
		else
			result += "struct " + project_name + "_" + win.wrapped.name + "_t : public layout_window_element {\n";

		result += "// BEGIN " + win.wrapped.name + "::variables\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::variables"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";

		for(auto& dm : win.wrapped.members) {
			result += "\t" + dm.type + " " + dm.name + ";\n";
		}
		result += "\tankerl::unordered_dense::map<std::string, std::unique_ptr<ui::lua_scripted_element>> scripted_elements;\n";
		for(auto& c : win.children) {
			if (!is_lua_element(c))
				result += "\t" "std::unique_ptr<" + element_class_name(project_name, win, c) + "> " + c.name + ";\n";
		}
		auto gens = make_generators_list(win.layout);
		for(auto g : gens) {
			result += "\t" + project_name + "_" + win.wrapped.name + "_" + g->name + "_t " + g->name + ";\n";
		}
		result += "\t" "std::vector<std::unique_ptr<ui::element_base>> gui_inserts;\n";

		auto wtables = tables_in_window(proj, win);
		for(auto t : wtables) {
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					if(col.display_data.header_key.size() > 0)
						result += "\t"  "std::string_view " + t->name + "_" + col.internal_data.column_name + "_header_text_key;\n";
					if(col.display_data.header_tooltip_key.size() > 0 && !col.internal_data.has_dy_header_tooltip)
						result += "\t"  "std::string_view " + t->name + "_" + col.internal_data.column_name + "_header_tooltip_key;\n";

					if(t->template_id == -1) {
						result += "\t"  "text::text_color " + t->name + "_" + col.internal_data.column_name + "_header_text_color = text::text_color::" + color_to_name(col.display_data.header_text_color) + ";\n";
						result += "\t"  "text::text_color " + t->name + "_" + col.internal_data.column_name + "_column_text_color = text::text_color::" + color_to_name(col.display_data.cell_text_color) + ";\n";
					} else {
						result += "\t"  "uint8_t " + t->name + "_" + col.internal_data.column_name + "_header_text_color = " + std::to_string(int32_t(col.display_data.header_text_color)) + ";\n";
						result += "\t"  "uint8_t " + t->name + "_" + col.internal_data.column_name + "_column_text_color = " + std::to_string(int32_t(col.display_data.cell_text_color)) + ";\n";
					}
					result += "\t" "text::alignment " + t->name + "_" + col.internal_data.column_name + "_text_alignment = text::alignment::" + alignment_to_name(col.display_data.text_alignment) + ";\n";

					if(col.display_data.cell_tooltip_key.length() > 0 && !col.internal_data.has_dy_cell_tooltip) {
						result += "\t"  "std::string_view " + t->name + "_" + col.internal_data.column_name + "_column_tooltip_key;\n";
					}
					if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
						result += "\t" "float " + t->name + "_" + col.internal_data.column_name + "_decimal_pos = 0.0f;";
					}
					if(col.internal_data.sortable) {
						result += "\t"  "int8_t " + t->name + "_" + col.internal_data.column_name + "_sort_direction = 0;\n";
					}
				}
				result += "\t"  "int16_t " + t->name + "_" + col.internal_data.column_name + "_column_start = 0;\n";
				result += "\t"  "int16_t " + t->name + "_" + col.internal_data.column_name + "_column_width = 0;\n";
			}

			if(t->ascending_sort_icon.length() > 0) {
				result += "\t"  "std::string_view " + t->name + "_ascending_icon_key;\n";
				result += "\t"  "dcon::texture_id " + t->name + "_ascending_icon;\n";
			}
			if(t->descending_sort_icon.length() > 0) {
				result += "\t"  "std::string_view " + t->name + "_descending_icon_key;\n";
				result += "\t"  "dcon::texture_id " + t->name + "_descending_icon;\n";
			}
			if(t->template_id == -1) {
				result += "\t" "ogl::color3f " + t->name + "_divider_color{float(" + std::to_string(t->divider_color.r) + "), float(" + std::to_string(t->divider_color.g) + "), float(" + std::to_string(t->divider_color.b) + ")};\n";
			}
		}

		if(win.wrapped.background == background_type::existing_gfx && win.wrapped.template_id == -1) {
			result += "\t"  "std::string_view gfx_key;\n";
			result += "\t"  "dcon::gfx_object_id background_gid;\n";
			result += "\t"  "int32_t frame = 0;\n";
		} else if(win.wrapped.background != background_type::none && win.wrapped.template_id == -1) {
			result += "\t"  "std::string_view texture_key;\n";
			if(win.wrapped.has_alternate_bg) {
				result += "\t"  "std::string_view alt_texture_key;\n";
				result += "\t"  "dcon::texture_id alt_background_texture;\n";
				result += "\t"  "bool is_active = false;\n";
			}
			result += "\t"  "dcon::texture_id background_texture;\n";
			if(win.wrapped.background == background_type::bordered_texture) {
				result += "\t"  "int16_t border_size = 0;\n";
			}
		}

		if(win.layout.contents.size() > 0)
			result += "\t" "void create_layout_level(sys::state& state, layout_level& lvl, char const* ldata, size_t sz);\n";
		result += "\t" "void on_create(sys::state& state) noexcept override;\n";
		if(win.wrapped.template_id == -1)
			result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";
		if(win.wrapped.template_id != -1 && !win.alternates.empty())
			result += "\t" "void set_alternate(bool alt) noexcept;\n";
		if(win.wrapped.on_hide_action) 
			result += "\t" "void on_hide(sys::state& state) noexcept override;\n";

		if(win.wrapped.background != background_type::none || win.wrapped.template_id != -1) {
			result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
			result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
		}

		if(win.wrapped.template_id != -1) {
			if(any_layout_paged(win.layout)) {
				result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
				result += "\t" "\t" "return ui::message_result::consumed;\n";
				result += "\t" "}\n";
			}
		} else {
			result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
			if(win.wrapped.background != background_type::none) {
				if(any_layout_paged(win.layout))
					result += "\t" "\t" "return ui::message_result::consumed;\n";
				else
					result += "\t" "\t" "return (type == ui::mouse_probe_type::scroll ? ui::message_result::unseen : ui::message_result::consumed);\n";
			} else {
				result += "\t" "\t" "return ui::message_result::unseen;\n";
			}
			result += "\t" "}\n";
		}

		if(win.wrapped.draggable) {
			result += "\t" "void on_drag(sys::state& state, int32_t oldx, int32_t oldy, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override {\n";
			result += "\t"  "\t" "auto location_abs = get_absolute_location(state, *this);\n";
			result += "\t"  "\t" "if(location_abs.x <= oldx && oldx < base_data.size.x + location_abs.x && location_abs.y <= oldy && oldy < base_data.size.y + location_abs.y) {\n";
			result += "\t"  "\t"  "\t" "ui::xy_pair new_abs_pos = location_abs;\n";
			result += "\t"  "\t"  "\t" "new_abs_pos.x += int16_t(x - oldx);\n";
			result += "\t"  "\t"  "\t" "new_abs_pos.y += int16_t(y - oldy);\n";
			result += "\t"  "\t"  "\t" "if(ui::ui_width(state) > base_data.size.x)\n";
			result += "\t"  "\t"  "\t" "\t" "new_abs_pos.x = int16_t(std::clamp(int32_t(new_abs_pos.x), 0, ui::ui_width(state) - base_data.size.x));\n";
			result += "\t"  "\t"  "\t" "if(ui::ui_height(state) > base_data.size.y)\n";
			result += "\t"  "\t"  "\t" "\t" "new_abs_pos.y = int16_t(std::clamp(int32_t(new_abs_pos.y), 0, ui::ui_height(state) - base_data.size.y));\n";

			result += "\t"  "\t"  "\t" "if(state_is_rtl(state)) {\n";
			result += "\t"  "\t"  "\t" "\t" "base_data.position.x -= int16_t(new_abs_pos.x - location_abs.x);\n";
			result += "\t"  "\t"  "\t" "} else {\n";
			result += "\t"  "\t"  "\t" "\t" "base_data.position.x += int16_t(new_abs_pos.x - location_abs.x);\n";
			result += "\t"  "\t"  "\t" "}\n";
			result += "\t"  "\t"  "base_data.position.y += int16_t(new_abs_pos.y - location_abs.y);\n";
			result += "\t" "\t" "}\n";
			result += "\t"  "}\n";
		}

		result += "\t"  "void on_update(sys::state& state) noexcept override;\n";

		if(!win.wrapped.members.empty()) {
			result += "\t" "void* get_by_name(sys::state& state, std::string_view name_parameter) noexcept override {\n";
			for(auto& m : win.wrapped.members) {
				result += "\t" "\t" "if(name_parameter == \"" + m.name + "\") {\n";
				result += "\t" "\t" "\t" "return (void*)(&" + m.name + ");\n";
				result += "\t" "\t" "}\n";
			}
			result += "\t" "\t" "return nullptr;\n";
			result += "\t" "}\n";
		}

		result += "};\n";

		result += "std::unique_ptr<ui::element_base> make_" + project_name + "_" + win.wrapped.name + "(sys::state& state);\n";
	}

	// type function implementations
	for(auto& win : proj.windows) {
		auto make_parent_var_text = [&](bool skip_self = false) {
			window_element_t const* w = &win.wrapped;
			std::string parent_string = "parent";
			while(w != nullptr) {
				if(skip_self) {

				} else {
					result += "\t" + project_name + "_" + w->name + "_t& " + w->name + " = *((" + project_name + "_" + w->name + "_t*)(" + parent_string + ")); \n";
				}
				if(w->parent.size() > 0) {
					bool found = false;
					bool from_layout = w->parent_is_layout;
					for(auto& wwin : proj.windows) {
						if(wwin.wrapped.name == w->parent) {
							w = &wwin.wrapped;
							found = true;
							break;
						}
					}
					if(!found)
						w = nullptr;
					if(skip_self || from_layout) {
						skip_self = false;
						parent_string += "->parent";
					} else {
						parent_string += "->parent->parent";
					}

				} else {
					w = nullptr;
				}
			}
		};
		auto make_sub_obj_parent_var_text = [&](ui_element_t& e) {
			window_element_t const* w = &win.wrapped;
			std::string parent_string = "parent";
			while(w != nullptr) {
				result += "\t" + project_name + "_" + w->name + "_" + e.name + "_t& " + e.name + " = *((" + project_name + "_" + w->name + "_" + e.name + "_t*)(" + parent_string + ")); \n";
				parent_string += "->parent";
				result += "\t" + project_name + "_" + w->name + "_t& " + w->name + " = *((" + project_name + "_" + w->name + "_t*)(" + parent_string + ")); \n";

				if(w->parent.size() > 0) {
					bool found = false;
					bool from_layout = w->parent_is_layout;
					for(auto& wwin : proj.windows) {
						if(wwin.wrapped.name == w->parent) {
							w = &wwin.wrapped;
							found = true;
							break;
						}
					}
					if(!found)
						w = nullptr;

					if( from_layout) {
						parent_string += "->parent";
					} else {
						parent_string += "->parent->parent";
					}
				} else {
					w = nullptr;
				}
			}
		};

		// generator functions
		auto gens = make_generators_list(win.layout);

		for(auto g : gens) {
			std::string insert_params;
			std::string insert_vals;
			for(auto& inserts : g->inserts) {
				insert_vals.clear();
				insert_params.clear();
				bool found = false;
				for(auto& window : proj.windows) {
					if(window.wrapped.name == inserts.name) {
						for(auto& dm : window.wrapped.members) {
							insert_params += dm.type + " " + dm.name + ", ";
							insert_vals += dm.name + ", ";
						}
						found = true;
					}
				}
				if(insert_params.size() > 2) {
					insert_params.pop_back();
					insert_params.pop_back();
				}
				if(insert_vals.size() > 2) {
					insert_vals.pop_back();
					insert_vals.pop_back();
				}
				if(found) {
					result += "void " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t::add_" + inserts.name + "(" + insert_params + ") {\n";
					result += "\t" "values.emplace_back(" + inserts.name + "_option{" + insert_vals + "});\n";
					result += "}\n";
				}
			}

			result += "void  " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t::on_create(sys::state& state, layout_window_element* parent) {\n";
			make_parent_var_text();
			result += "// BEGIN " + win.wrapped.name + "::" + g->name + "::on_create\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::" + g->name + "::on_create"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";
			result += "}\n";

			result += "void  " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t::update(sys::state& state, layout_window_element* parent) {\n";
			make_parent_var_text();
			result += "// BEGIN " + win.wrapped.name + "::" + g->name + "::update\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::" + g->name + "::update"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";
			for(auto& i : g->inserts) {
				if(i.sortable) {
					if(auto w = window_from_name(proj, i.name);  w) {
						if(auto t = table_from_name(proj, w->wrapped.table_connection); t) {

							result += "\t" "{\n"; // scope
							result += "\t" "bool work_to_do = false;\n";
							result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.name + "_t*)(parent);\n";
							for(auto& col : t->table_columns) {
								if(col.internal_data.sortable)
									result += "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction != 0) work_to_do = true;\n";
							}
							result += "\t" "if(work_to_do) {\n"; // if work to do
							// sort ranges of insert
							result += "\t" "\t" "for(size_t i = 0; i < values.size(); ) {\n"; // loop over values
							result += "\t" "\t" "\t" "if(std::holds_alternative<" + i.name + "_option>(values[i])) {\n"; // if holds type
							result += "\t" "\t" "\t" "\t" "auto start_i = i;\n";
							result += "\t" "\t" "\t" "\t" "while(i < values.size() && std::holds_alternative<" + i.name + "_option>(values[i])) ++i;\n";

							for(auto& col : t->table_columns) {
								if(col.internal_data.sortable) {
									result += "\t" "\t" "\t" "\t" "if(table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction != 0) {\n";
									result +=  "\t" "\t" "\t" "\t" "\t" "sys::merge_sort(values.begin() + start_i, values.begin() + i, [&](auto const& raw_a, auto const& raw_b){\n";
									result +=  "\t" "\t" "\t" "\t" "\t" "\t" "auto const& a = std::get<" + i.name + "_option>(raw_a);\n";
									result +=  "\t" "\t" "\t" "\t" "\t" "\t" "auto const& b = std::get<" + i.name + "_option>(raw_b);\n";
									result +=  "\t" "\t" "\t" "\t" "\t" "\t" "int8_t result = 0;\n";
									result += "// BEGIN " + win.wrapped.name + "::" + g->name + "::" + t->name + "::sort::" + col.internal_data.column_name + "\n";
									if(auto it = old_code.found_code.find(win.wrapped.name + "::" + g->name + "::" + t->name + "::sort::" + col.internal_data.column_name); it != old_code.found_code.end()) {
										it->second.used = true;
										result += it->second.text;
									}
									result += "// END\n";
									result += "\t" "\t" "\t" "\t" "\t" "\t" "return -result == table_source->" + t->name + "_" + col.internal_data.column_name + "_sort_direction;\n";
									result += "\t" "\t" "\t" "\t" "\t" "});\n";
									result += "\t" "\t" "\t" "\t" "}\n";
								}
							}


							result += "\t" "\t" "\t" "} else {\n"; // else not holds type
							result += "\t" "\t" "\t" "\t" "++i;\n";
							result += "\t" "\t" "\t" "}\n"; // end type if
							result += "\t" "\t" "}\n"; // end loop over values
							result += "\t" "}\n"; // end if work to do
							result += "\t" "}\n"; // end scope
						}
					}
				}
			}
			result += "}\n";

			result += "measure_result  " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t::place_item(sys::state& state, ui::non_owning_container_base* destination, size_t index, int32_t x, int32_t y, bool first_in_section, bool& alternate) {\n";

			auto special_type_to_str = [](glue_type t) {
				switch(t) {
					case glue_type::standard: return "none";
					case glue_type::at_least: return "space_consumer";
					case glue_type::line_break: return "end_line";
					case glue_type::page_break: return "end_page";
					case glue_type::glue_don_t_break: return "no_break";
				}
				return "none";
			};

			result += "\t" "if(index >= values.size()) return measure_result{0,0,measure_result::special::none};\n";
			for(auto& inserts : g->inserts) {
				if(auto w = window_from_name(proj, inserts.name);  !w)
					continue;

				std::string shared_header_test;
				for(auto& oinserts : g->inserts) {
					if(oinserts.header == inserts.header) {
						shared_header_test += " && !std::holds_alternative<" + oinserts.name + "_option>(values[index - 1])";
					}
				}

				result += "\t" "if(std::holds_alternative<" + inserts.name + "_option>(values[index])) {\n";
				if(inserts.header.size() > 0)
					result += "\t" "\t" "if(" + inserts.header + "_pool.empty()) " + inserts.header + "_pool.emplace_back(make_" + project_name + "_" + inserts.header + "(state));\n";
				result += "\t" "\t" "if(" + inserts.name + "_pool.empty()) " + inserts.name + "_pool.emplace_back(make_" + project_name + "_" + inserts.name + "(state));\n";

				if(inserts.header.size() > 0) {
					result += "\t" "\t" "if(index == 0 || first_in_section || (true" + shared_header_test + ")) {\n";
					result += "\t" "\t" "\t" "if(destination) {\n";

					result += "\t" "\t" "\t" "\t" "if(" + inserts.header + "_pool.size() <= size_t(" + inserts.header + "_pool_used)) " + inserts.header + "_pool.emplace_back(make_" + project_name + "_" + inserts.header + "(state));\n";
					result += "\t" "\t" "\t" "\t" "if(" + inserts.name + "_pool.size() <= size_t(" + inserts.name + "_pool_used)) " + inserts.name + "_pool.emplace_back(make_" + project_name + "_" + inserts.name + "(state));\n";

					result += "\t" "\t" "\t" "\t" + inserts.header + "_pool[" + inserts.header + "_pool_used]->base_data.position.x = int16_t(x);\n";
					result += "\t" "\t" "\t" "\t" + inserts.header + "_pool[" + inserts.header + "_pool_used]->base_data.position.y = int16_t(y);\n";
					result += "\t" "\t" "\t" "\t" "if(!" + inserts.header + "_pool[" + inserts.header + "_pool_used]->parent) {\n";
					result += "\t" "\t" "\t" "\t" "\t" + inserts.header + "_pool[" + inserts.header + "_pool_used]->parent = destination;\n";
					result += "\t" "\t" "\t" "\t" "\t" + inserts.header + "_pool[" + inserts.header + "_pool_used]->impl_on_update(state);\n";
					result += "\t" "\t" "\t" "\t" "\t" + inserts.header + "_pool[" + inserts.header + "_pool_used]->impl_on_reset_text(state);\n";
					result += "\t" "\t" "\t" "\t" "}\n";
					result += "\t" "\t" "\t" "\t" "destination->children.push_back(" + inserts.header + "_pool[" + inserts.header + "_pool_used].get());\n";

					for(auto& window : proj.windows) {
						if(window.wrapped.name == inserts.header) {
							if(window.wrapped.template_id != -1 && !window.alternates.empty()) {
								result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.header + "_t*)(" + inserts.header + "_pool[" + inserts.header + "_pool_used].get()))->set_alternate(alternate);\n";
							} else if(window.wrapped.has_alternate_bg) {
								result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.header + "_t*)(" + inserts.header + "_pool[" + inserts.header + "_pool_used].get()))->is_active = alternate;\n";
							}
						}
					}

					result += "\t" "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->base_data.position.x = int16_t(x);\n";
					result += "\t" "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->base_data.position.y = int16_t(y +  " + inserts.name + "_pool[0]->base_data.size.y + " + std::to_string(inserts.inter_item_space) + ");\n";
					result += "\t" "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->parent = destination;\n";
					result += "\t" "\t" "\t" "\t" "destination->children.push_back(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get());\n";

					for(auto& window : proj.windows) {
						if(window.wrapped.name == inserts.name) {
							for(auto& dm : window.wrapped.members) {
								result += "\t" "\t" "\t"  "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->" + dm.name + " = std::get<" + inserts.name + "_option>(values[index])." + dm.name + ";\n";
							}
							if(window.wrapped.template_id != -1 && !window.alternates.empty()) {
								result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->set_alternate(!alternate);\n";
							} else if(window.wrapped.has_alternate_bg) {
								result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->is_active = !alternate;\n";
							}
						}
					}
					result += "\t" "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->impl_on_update(state);\n";


					result += "\t" "\t" "\t" "\t" + inserts.header + "_pool_used++;\n";
					result += "\t" "\t" "\t" "\t" + inserts.name + "_pool_used++;\n";
					result += "\t" "\t" "\t" "}\n";

					result += "\t" "\t" "\t" "return measure_result{std::max(" + inserts.header + "_pool[0]->base_data.size.x, " + inserts.name + "_pool[0]->base_data.size.x), " + inserts.header + "_pool[0]->base_data.size.y + " + inserts.name + "_pool[0]->base_data.size.y + " + std::to_string(inserts.inter_item_space * 2) + ", measure_result::special::" + std::string(special_type_to_str(inserts.glue)) + "};\n";
					result += "\t" "\t" "}\n";
				}

				result += "\t" "\t" "if(destination) {\n";

				result += "\t" "\t" "\t" "if(" + inserts.name + "_pool.size() <= size_t(" + inserts.name + "_pool_used)) " + inserts.name + "_pool.emplace_back(make_" + project_name + "_" + inserts.name + "(state));\n";

				result += "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->base_data.position.x = int16_t(x);\n";
				result += "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->base_data.position.y = int16_t(y);\n";
				result += "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->parent = destination;\n";
				result += "\t" "\t" "\t" "destination->children.push_back(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get());\n";

				for(auto& window : proj.windows) {
					if(window.wrapped.name == inserts.name) {
						for(auto& dm : window.wrapped.members) {
							result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->" + dm.name + " = std::get<" + inserts.name + "_option>(values[index])." + dm.name + ";\n";
						}
						if(window.wrapped.template_id != -1 && !window.alternates.empty()) {
							result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->set_alternate(alternate);\n";
						} else if(window.wrapped.has_alternate_bg) {
							result += "\t" "\t" "\t" "((" + project_name + "_" + inserts.name + "_t*)(" + inserts.name + "_pool[" + inserts.name + "_pool_used].get()))->is_active = alternate;\n";
						}
					}
				}
				result += "\t" "\t" "\t" + inserts.name + "_pool[" + inserts.name + "_pool_used]->impl_on_update(state);\n";
				result += "\t" "\t" "\t" + inserts.name + "_pool_used++;\n";

				// add header and body
				result += "\t" "\t" "}\n";

				if(auto w = window_from_name(proj, inserts.name); w) {
					if(w->wrapped.has_alternate_bg || (w->wrapped.template_id != -1 && !w->alternates.empty()))
						result += "\t" "\t" "alternate = !alternate;\n";
					else
						result += "\t" "\t" "alternate = true;\n";
				}
				result += "\t" "\t" "return measure_result{ " + inserts.name + "_pool[0]->base_data.size.x, " + inserts.name + "_pool[0]->base_data.size.y + " + std::to_string(inserts.inter_item_space) + ", measure_result::special::" + std::string(special_type_to_str(inserts.glue)) + "};\n";
				result += "\t" "}\n";
			}
			result += "\t" "return measure_result{0,0,measure_result::special::none};\n";
			result += "}\n";

			result += "void  " + project_name + "_" + win.wrapped.name + "_" + g->name + "_t::reset_pools() {\n";
			for(auto& inserts : g->inserts) {
				if(auto w = window_from_name(proj, inserts.name);  !w)
					continue;
				if(inserts.header.size() > 0)
					result += "\t" + inserts.header + "_pool_used = 0;\n";
				result += "\t" + inserts.name + "_pool_used = 0;\n";
			}
			result += "}\n";
		}

		for(auto& c : win.children) { // child functions
			if (is_lua_element(c))
				continue;

			result += element_member_functions(project_name, win, c, old_code, proj);
		}

		// window functions
		if(win.wrapped.template_id != -1 && !win.alternates.empty()) {
			result += "void  " + project_name + "_" + win.wrapped.name + "_t::set_alternate(bool alt) noexcept {\n";
			for(auto& alt : win.alternates) {
				if(alt.control_name == "") {
					result += "\t" "window_template = alt ? " + std::to_string(alt.tempalte_id) + " : " + std::to_string(win.wrapped.template_id) + ";\n";
				} else {
					for(auto& c : win.children) {
						if(c.name == alt.control_name) {
							result += "\t"  + c.name+ "->template_id = alt ? " + std::to_string(alt.tempalte_id) + " : " + std::to_string(c.template_id) + ";\n";
							break;
						}
					}
				}
			}
			result += "}\n";
		}

		if(win.wrapped.background != background_type::none || win.wrapped.template_id != -1) {
			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
			if(win.wrapped.draggable) {
				result += "\t" "state.ui_state.drag_target = this;\n";
			}
			result += "\t" "return ui::message_result::consumed;\n";
			result += "}\n";

			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
			result += "\t" "return ui::message_result::consumed;\n";
			result += "}\n";

			if(win.wrapped.template_id == -1) {
				result += "void " + project_name + "_" + win.wrapped.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				if(win.wrapped.background == background_type::existing_gfx) {
					result += "\t" "if(background_gid) {\n";
					result += "\t" "\t" "auto& gfx_def = state.ui_defs.gfx[background_gid];\n";
					result += "\t" "\t" "if(gfx_def.primary_texture_handle) {\n";
					result += "\t" "\t" "\t" "if(gfx_def.get_object_type() == ui::object_type::bordered_rect) {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), gfx_def.type_dependent, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "\t" "} else if(gfx_def.number_of_frames > 1) {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), frame, gfx_def.number_of_frames, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "\t" "} else {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped()," + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "\t" "}\n";
					result += "\t" "\t" "}\n";
					result += "\t" "}\n";
				} else if(win.wrapped.background == background_type::texture) {
					if(win.wrapped.has_alternate_bg) {
						result += "\t" "if(is_active)\n";
						result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
						result += "\t" "else\n";
						result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
					} else {
						result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
					}
				} else if(win.wrapped.background == background_type::bordered_texture) {
					if(win.wrapped.has_alternate_bg) {
						result += "\t" "if(is_active)\n";
						result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
						result += "\t" "else\n";
						result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
					} else {
						result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
					}
				} else if(win.wrapped.background == background_type::border_texture_repeat) {
					result += "\t" "ogl::render_rect_with_repeated_border(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(" + std::to_string(proj.grid_size) + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				} else if(win.wrapped.background == background_type::textured_corners) {
					result += "\t" "ogl::render_rect_with_repeated_corner(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(" + std::to_string(proj.grid_size) + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + "); \n";
				}

				// render all textures from layout:
				if(!win.layout.contents.empty()) {
					result += "\t" "auto cmod = ui::get_color_modification(false, false,  false);" "\n";
					result += "\t" "for (auto& _item : textures_to_render) {" "\n";
					result += "\t" "\t" "if (_item.texture_type == background_type::texture)" "\n";
					result += "\t" "\t" "\t" "ogl::render_textured_rect(state, cmod, float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "else if (_item.texture_type == background_type::border_texture_repeat)" "\n";
					result += "\t" "\t" "\t" "ogl::render_rect_with_repeated_border(state, cmod, float(" + std::to_string(proj.grid_size) + "), float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "else if (_item.texture_type == background_type::textured_corners)" "\n";
					result += "\t" "\t" "\t" "ogl::render_rect_with_repeated_corner(state, cmod, float(" + std::to_string(proj.grid_size) + "), float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "}" "\n";
				}

				if(win.wrapped.share_table_highlight) {
					auto t = table_from_name(proj, win.wrapped.table_connection);
					if(t) {
						if(t->has_highlight_color) {
							result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent);\n";

							result += "\t" "auto under_mouse = [&](){auto p = state.ui_state.under_mouse; while(p){ if(p == this) return true; p = p->parent; } return false;}();\n";
							result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - ui::get_absolute_location(state, *this).x;\n";

							result += "\t" "if(under_mouse) {\n";
							result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x), float(y), float(base_data.size.x), float(base_data.size.y), " + std::to_string(t->highlight_color.r) + "f, " + std::to_string(t->highlight_color.g) + "f, " + std::to_string(t->highlight_color.b) + "f, " + std::to_string(t->highlight_color.a) + "f);\n";
							result += "\t" "}\n";

							for(auto& col : t->table_columns) {
								if(col.internal_data.cell_type == table_cell_type::text) {
									result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
									result += "\t" "if(col_um_" + col.internal_data.column_name + " && !under_mouse) {\n";
									result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(base_data.size.y), " + std::to_string(t->highlight_color.r) + "f, " + std::to_string(t->highlight_color.g) + "f, " + std::to_string(t->highlight_color.b) + "f, " + std::to_string(t->highlight_color.a) + "f);\n";
									result += "\t" "}\n";
								}
							}
						}
					}
				}

				result += "}\n";
			}
		} else {
			if(win.wrapped.template_id == -1) {
				result += "void " + project_name + "_" + win.wrapped.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";

				// render all textures from layout:
				if(!win.layout.contents.empty()) {
					result += "\t" "auto cmod = ui::get_color_modification(false, false,  false);" "\n";
					result += "\t" "for (auto& _item : textures_to_render) {" "\n";
					result += "\t" "\t" "if (_item.texture_type == background_type::texture)" "\n";
					result += "\t" "\t" "\t" "ogl::render_textured_rect(state, cmod, float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "else if (_item.texture_type == background_type::border_texture_repeat)" "\n";
					result += "\t" "\t" "\t" "ogl::render_rect_with_repeated_border(state, cmod, float(" + std::to_string(proj.grid_size) + "), float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "\t" "else if (_item.texture_type == background_type::textured_corners)" "\n";
					result += "\t" "\t" "\t" "ogl::render_rect_with_repeated_corner(state, cmod, float(" + std::to_string(proj.grid_size) + "), float(x + _item.x), float(y + _item.y), float(_item.w), float(_item.h), ogl::get_late_load_texture_handle(state, _item.texture_id, _item.texture), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state_is_rtl(state)") + ");\n";
					result += "\t" "}" "\n";
				}

				if(win.wrapped.share_table_highlight) {
					auto t = table_from_name(proj, win.wrapped.table_connection);
					if(t) {
						if(t->has_highlight_color) {
							result += "\t" "auto table_source = (" + project_name + "_" + win.wrapped.parent + "_t*)(parent);\n";

							result += "\t" "auto under_mouse = [&](){auto p = state.ui_state.under_mouse; while(p){ if(p == this) return true; p = p->parent; } return false;}();\n";
							result += "\t" "int32_t rel_mouse_x = int32_t(state.mouse_x_position / state.user_settings.ui_scale) - ui::get_absolute_location(state, *this).x;\n";

							result += "\t" "if(under_mouse) {\n";
							result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x), float(y), float(base_data.size.x), float(base_data.size.y), " + std::to_string(t->highlight_color.r) + "f, " + std::to_string(t->highlight_color.g) + "f, " + std::to_string(t->highlight_color.b) + "f, " + std::to_string(t->highlight_color.a) + "f);\n";
							result += "\t" "}\n";

							for(auto& col : t->table_columns) {
								if(col.internal_data.cell_type == table_cell_type::text) {
									result += "\t" "bool col_um_" + col.internal_data.column_name + " = rel_mouse_x >= table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start && rel_mouse_x < (table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
									result += "\t" "if(col_um_" + col.internal_data.column_name + " && !under_mouse) {\n";
									result += "\t" "\t" "ogl::render_alpha_colored_rect(state, float(x + table_source->" + t->name + "_" + col.internal_data.column_name + "_column_start), float(y), float(table_source->" + t->name + "_" + col.internal_data.column_name + "_column_width), float(base_data.size.y), " + std::to_string(t->highlight_color.r) + "f, " + std::to_string(t->highlight_color.g) + "f, " + std::to_string(t->highlight_color.b) + "f, " + std::to_string(t->highlight_color.a) + "f);\n";
									result += "\t" "}\n";
								}
							}
						}
					}
				}

				result += "}\n";
			}
		}


		//HIDE/CLOSE
		if(win.wrapped.on_hide_action) {
			result += "void " + project_name + "_" + win.wrapped.name + "_t::on_hide(sys::state& state) noexcept {\n";
			make_parent_var_text(true);
			result += "// BEGIN " + win.wrapped.name + "::on_hide\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::on_hide"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";
			result += "}\n";
		}

		//UPDATE
		result += "void " + project_name + "_" + win.wrapped.name + "_t::on_update(sys::state& state) noexcept {\n";
		make_parent_var_text(true);
		result += "// BEGIN " + win.wrapped.name + "::update\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::update"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";

		auto wtables = tables_in_window(proj, win);
		for(auto t : wtables) {
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					if(col.internal_data.decimal_alignment != aui_text_alignment::center) {
						result += "\t" "{\n";
						result += "\t" "bool left_align = " + std::string(col.internal_data.decimal_alignment == aui_text_alignment::right ? "true" : "false") + " == (state_is_rtl(state)); \n";
						result += "\t" "if(left_align)\n";
						result += "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = 1000000.0f;\n";
						result += "\t" "else\n";
						result += "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_decimal_pos = -1000000.0f;\n";
						result += "\t" "}\n";
					}
				}
			}
		}

		for(auto g : gens) {
			result += "\t" + g->name + ".update(state, this);\n";
		}
		if(win.layout.contents.size() > 0)
			result += "\t" "remake_layout(state, true);\n";
		result += "}\n";


		if(win.layout.contents.size() > 0) {
			result += "void " + project_name + "_" + win.wrapped.name + "_t::create_layout_level(sys::state& state, layout_level& lvl, char const* ldata, size_t sz) {\n";
			result += "\t" "serialization::in_buffer buffer(ldata, sz);\n";
			result += "\t" "buffer.read(lvl.size_x); \n";
			result += "\t" "buffer.read(lvl.size_y); \n";
			result += "\t" "buffer.read(lvl.margin_top); \n";
			result += "\t" "buffer.read(lvl.margin_bottom); \n";
			result += "\t" "buffer.read(lvl.margin_left); \n";
			result += "\t" "buffer.read(lvl.margin_right); \n";
			result += "\t" "buffer.read(lvl.line_alignment); \n";
			result += "\t" "buffer.read(lvl.line_internal_alignment); \n";
			result += "\t" "buffer.read(lvl.type); \n";
			result += "\t" "buffer.read(lvl.page_animation); \n";
			result += "\t" "buffer.read(lvl.interline_spacing); \n";
			result += "\t" "buffer.read(lvl.paged); \n";
			result += "\t" "if(lvl.paged) {\n";
			result += "\t" "\t" "lvl.page_controls = std::make_unique<page_buttons>();\n";
			result += "\t" "\t" "lvl.page_controls->for_layout = &lvl;\n";
			result += "\t" "\t" "lvl.page_controls->parent = this;\n";
			result += "\t" "\t" "lvl.page_controls->base_data.size.x = int16_t(grid_size * 10);\n";
			result += "\t" "\t" "lvl.page_controls->base_data.size.y = int16_t(grid_size * 2);\n";
			result += "\t" "}\n";
			result += "\t" "auto expansion_section = buffer.read_section();\n";
			result += "\t" "if(expansion_section)\n";
			result += "\t" "\t" "expansion_section.read(lvl.template_id);\n";
			result += "\t" "if(lvl.template_id == -1 && window_template != -1)\n";
			result += "\t" "\t" "lvl.template_id = int16_t(state.ui_templates.window_t[window_template].layout_region_definition);\n";
			result += "\t" "while(buffer) {\n";
			result += "\t" "\t" "layout_item_types t;\n";
			result += "\t" "\t" "buffer.read(t);\n";
			result += "\t" "\t" "switch(t) {\n";
			result += "\t" "\t" "\t" "case layout_item_types::texture_layer:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "texture_layer temp;\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.texture_type);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.texture);\n";
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";
			result += "\t" "\t" "\t" "case layout_item_types::control:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "layout_control temp;\n";
			result += "\t" "\t" "\t" "\t" "std::string_view cname = buffer.read<std::string_view>();\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.abs_x);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.abs_y);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.absolute_position);\n";
			result += "\t" "\t" "\t" "\t" "temp.ptr = nullptr;\n";
			for(auto& c : win.children) {
				if(!is_lua_element(c)) {
					result += "\t" "\t" "\t" "\t" "if(cname == \"" + c.name + "\") {\n";
					result += "\t" "\t" "\t" "\t" "\t" "temp.ptr = " + c.name + ".get();\n";
					result += "\t" "\t" "\t" "\t" "} else\n";
				}
			}
			{
				result += "\t" "\t" "\t" "\t" "{\n";
				result += "\t" "\t" "\t" "\t" "\t" "std::string str_cname {cname};\n";
				result += "\t" "\t" "\t" "\t" "\t" "auto found = scripted_elements.find(str_cname);\n";
				result += "\t" "\t" "\t" "\t" "\t" "if (found != scripted_elements.end()) {\n";
				result += "\t" "\t" "\t" "\t" "\t" "\t" "temp.ptr = found->second.get();\n";
				result += "\t" "\t" "\t" "\t" "\t" "}\n";
				result += "\t" "\t" "\t" "\t" "}\n";
			}
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";

			result += "\t" "\t" "\t" "case layout_item_types::window:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "layout_window temp;\n";
			result += "\t" "\t" "\t" "\t" "std::string_view cname = buffer.read<std::string_view>();\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.abs_x);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.abs_y);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.absolute_position);\n";
			for(auto& c : proj.windows) {
				result += "\t" "\t" "\t" "\t" "if(cname == \"" + c.wrapped.name + "\") {\n";
				result += "\t" "\t" "\t" "\t" "\t" "temp.ptr = make_" + project_name + "_" + c.wrapped.name + "(state);\n";
				result += "\t" "\t" "\t" "\t" "}\n";
			}
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";

			result += "\t" "\t" "\t" "case layout_item_types::glue:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "layout_glue temp;\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.type);\n";
			result += "\t" "\t" "\t" "\t" "buffer.read(temp.amount);\n";
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";

			result += "\t" "\t" "\t" "case layout_item_types::generator:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "generator_instance temp;\n";
			result += "\t" "\t" "\t" "\t" "std::string_view cname = buffer.read<std::string_view>();\n";
			result += "\t" "\t" "\t" "\t"  "auto gen_details = buffer.read_section(); // ignored\n";
			for(auto g : gens) {
				result += "\t" "\t" "\t" "\t" "if(cname == \"" + g->name + "\") {\n";
				result += "\t" "\t" "\t" "\t" "\t" "temp.generator = &" + g->name + ";\n";
				result += "\t" "\t" "\t" "\t" "}\n";
			}
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";

			result += "\t" "\t" "\t" "case layout_item_types::layout:\n";
			result += "\t" "\t" "\t" "{\n";
			result += "\t" "\t" "\t" "\t" "sub_layout temp;\n";
			result += "\t" "\t" "\t" "\t" "temp.layout = std::make_unique<layout_level>();\n";
			result += "\t" "\t" "\t" "\t"  "auto layout_section = buffer.read_section();\n";
			result += "\t" "\t" "\t" "\t" "create_layout_level(state, *temp.layout, layout_section.view_data() + layout_section.view_read_position(), layout_section.view_size() - layout_section.view_read_position());\n";
			result += "\t" "\t" "\t" "\t" "lvl.contents.emplace_back(std::move(temp));\n";
			result += "\t" "\t" "\t" "} break;\n";

			result += "\t" "\t" "}\n"; // end switch

			result += "\t" "}\n"; // end loop over contents
			result += "}\n";
		}

		//ON CREATE
		result += "void " + project_name + "_" + win.wrapped.name + "_t::on_create(sys::state& state) noexcept {\n";

		result += "\t" "auto window_bytes = state.ui_state.new_ui_windows.find(std::string(\"" + project_name + "::" + win.wrapped.name + "\"));\n";
		result += "\t" "if(window_bytes == state.ui_state.new_ui_windows.end()) std::abort();\n";
		result += "\t" "std::vector<sys::aui_pending_bytes> pending_children;\n";
		result += "\t" "auto win_data = read_window_bytes(window_bytes->second.data, window_bytes->second.size, pending_children);\n";
		result += "\t" "base_data.position.x = win_data.x_pos;\n";
		result += "\t" "base_data.position.y = win_data.y_pos;\n";
		result += "\t" "base_data.size.x = win_data.x_size;\n";
		result += "\t" "base_data.size.y = win_data.y_size;\n";
		result += "\t" "base_data.flags = uint8_t(win_data.orientation);\n";

		if(win.wrapped.template_id != -1) {
			result += "\t" "layout_window_element::initialize_template(state, win_data.template_id, win_data.grid_size, win_data.auto_close_button);\n";
		} else {
			if(win.wrapped.background == background_type::existing_gfx) {
				result += "\t"  "gfx_key = win_data.texture;\n";
				result += "\t" "if(auto it = state.ui_state.gfx_by_name.find(state.lookup_key(gfx_key)); it != state.ui_state.gfx_by_name.end()) {\n";
				result += "\t" "\t" "background_gid = it->second;\n";
				result += "\t" "}\n";
			} else if(background_type_is_textured(win.wrapped.background)) {
				result += "\t"   "texture_key = win_data.texture;\n";
				if(background_type_requires_border_width(win.wrapped.background)) {
					result += "\t"  "\t"  "border_size = win_data.border_size;\n";
				}
				if(win.wrapped.has_alternate_bg) {
					result += "\t"   "alt_texture_key = win_data.alt_texture;\n";
				}
			}
		}

		if(win.wrapped.updates_while_hidden) {
			result += "\t"  "ui::element_base::flags |= ui::element_base::wants_update_when_hidden_mask;\n";
		}

		result += "\t" "while(!pending_children.empty()) {\n";
		result += "\t" "\t" "auto child_data = read_child_bytes(pending_children.back().data, pending_children.back().size);\n";
		for(auto& c : win.children) {
			if (is_lua_element(c)) {
				continue;
			}
			result += "\t" "\t" "if(child_data.name == \"" + c.name + "\") {\n";
			result += element_initialize_child(project_name, win, c, proj);
			result += "\t" "\t" "} else \n";
		}
		auto tabs = tables_in_window(proj, win);
		for(auto t : tabs) {
			result += "\t" "\t" "if(child_data.name == \".tab" + t->name + "\") {\n";
			result += "\t" "\t" "\t" "int16_t running_w_total = 0;\n";
			result += "\t" "\t" "\t" "auto tbuffer = serialization::in_buffer(pending_children.back().data, pending_children.back().size);\n";
			result += "\t" "\t" "\t" "auto main_section = tbuffer.read_section();\n";
			result += "\t" "\t" "\t" "main_section.read<std::string_view>(); // discard name \n";
			if(t->ascending_sort_icon.size() > 0)
				result += "\t" "\t" "\t" + t->name + "_ascending_icon_key = main_section.read<std::string_view>();\n";
			else
				result += "\t" "\t" "\t" "main_section.read<std::string_view>(); // discard\n";
			if(t->descending_sort_icon.size() > 0)
				result += "\t" "\t" "\t" + t->name + "_descending_icon_key = main_section.read<std::string_view>();\n";
			else
				result += "\t" "\t" "\t" "main_section.read<std::string_view>(); // discard\n";
			if(t->template_id != -1) {
				result += "\t" "\t" "\t" "main_section.read<ogl::color3f>();\n";
				// result += "\t" "\t" "\t" + t->name + "_template_id = main_section.read<int32_t>();\n";
				// result += "\t" "\t" "\t" + t->name + "_ink_color = state.ui_templates.tables_t[" + t->name + "_template_id].table_color;\n";
			} else {
				result += "\t" "\t" "\t" "main_section.read(" + t->name + "_divider_color);\n";
			}
			result += "\t" "\t" "\t" "auto col_section = tbuffer.read_section();\n";
			for(auto& col : t->table_columns) {
				if(col.internal_data.cell_type == table_cell_type::text) {
					if(col.display_data.header_key.size() > 0)
						result += "\t" "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_header_text_key = col_section.read<std::string_view>();\n";
					else
						result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";
					if(col.display_data.header_tooltip_key.size() > 0 && !col.internal_data.has_dy_header_tooltip)
						result += "\t" "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_header_tooltip_key = col_section.read<std::string_view>();\n";
					else
						result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";
					if(col.display_data.cell_tooltip_key.length() > 0 && !col.internal_data.has_dy_cell_tooltip)
						result += "\t" "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_column_tooltip_key = col_section.read<std::string_view>();\n";
					else
						result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";
					result += "\t" "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_column_start = running_w_total;\n";
					result += "\t" "\t" "\t" "col_section.read(" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
					result += "\t" "\t" "\t" "running_w_total += " + t->name + "_" + col.internal_data.column_name + "_column_width;\n";
					result += "\t" "\t" "\t" "col_section.read(" + t->name + "_" + col.internal_data.column_name + "_column_text_color);\n";
					result += "\t" "\t" "\t" "col_section.read(" + t->name + "_" + col.internal_data.column_name + "_header_text_color);\n";
					result += "\t" "\t" "\t" "col_section.read(" + t->name + "_" + col.internal_data.column_name + "_text_alignment);\n";
				} else {
					result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";
					result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";
					result += "\t" "\t" "\t" "col_section.read<std::string_view>(); // discard\n";

					result += "\t" "\t" "\t" + t->name + "_" + col.internal_data.column_name + "_column_start = running_w_total;\n";
					result += "\t" "\t" "\t" "col_section.read(" + t->name + "_" + col.internal_data.column_name + "_column_width);\n";
					result += "\t" "\t" "\t" "running_w_total += " + t->name + "_" + col.internal_data.column_name + "_column_width;\n";

					result += "\t" "\t" "\t" "col_section.read<text::text_color>(); // discard\n";
					result += "\t" "\t" "\t" "col_section.read<text::text_color>(); // discard\n";
					result += "\t" "\t" "\t" "col_section.read<text::alignment>(); // discard\n";
				}
			}
			result += "\t" "\t" "\t" "pending_children.pop_back(); continue;\n";
			result += "\t" "\t" "} else \n";
		}
		{
			result += "\t" "\t" "if (child_data.is_lua) { \n";
			result += "\t" "\t" "\t" "std::string str_name {child_data.name};\n";
			result += "\t" "\t" "\t" "scripted_elements[str_name] = std::make_unique<ui::lua_scripted_element>();\n";
			result += "\t" "\t" "\t" "auto cptr = scripted_elements[str_name].get();\n";
			result += "\t" "\t" "\t" "cptr->base_data.position.x = child_data.x_pos;\n";
			result += "\t" "\t" "\t" "cptr->base_data.position.y = child_data.y_pos;\n";
			result += "\t" "\t" "\t" "cptr->base_data.size.x = child_data.x_size;\n";
			result += "\t" "\t" "\t" "cptr->base_data.size.y = child_data.y_size;\n";
			result += "\t" "\t" "\t" "cptr->texture_key = child_data.texture;\n";
			result += "\t" "\t" "\t" "cptr->text_scale = child_data.text_scale;\n";
			result += "\t" "\t" "\t" "cptr->text_is_header = (child_data.text_type == aui_text_type::header);\n";
			result += "\t" "\t" "\t" "cptr->text_alignment = child_data.text_alignment;\n";
			result += "\t" "\t" "\t" "cptr->text_color = child_data.text_color;\n";
			result += "\t" "\t" "\t" "cptr->on_update_lname = child_data.text_key;\n";
			result += "\t" "\t" "\t" "if(child_data.tooltip_text_key.length() > 0) {\n";
			result += "\t" "\t" "\t" "\t" "cptr->tooltip_key = state.lookup_key(child_data.tooltip_text_key);\n";
			result += "\t" "\t" "\t" "}\n";
			result += "\t" "\t" "\t" "cptr->parent = this;\n";
			result += "\t" "\t" "\t" "cptr->on_create(state);\n";
			result += "\t" "\t" "\t" "children.push_back(cptr);\n";
			result += "\t" "\t" "\t" "pending_children.pop_back(); continue;\n";
			result += "\t" "\t" "}\n";
		}
		result += "\t" "\t" "pending_children.pop_back();\n";
		result += "\t" "}\n";

		for(auto g : gens) {
			result += "\t" + g->name + ".on_create(state, this);\n";
		}

		if(win.layout.contents.size() > 0) {
			result += "\t" "page_left_texture_key = win_data.page_left_texture;\n";
			result += "\t" "page_right_texture_key = win_data.page_right_texture;\n";
			result += "\t" "page_text_color = win_data.page_text_color;\n";
			result += "\t" "create_layout_level(state, layout, win_data.layout_data, win_data.layout_data_size);\n";
		}

		result += "// BEGIN " + win.wrapped.name + "::create\n";
		if(auto it = old_code.found_code.find(win.wrapped.name + "::create"); it != old_code.found_code.end()) {
			it->second.used = true;
			result += it->second.text;
		}
		result += "// END\n";

		result += "}\n";

		result += "std::unique_ptr<ui::element_base> make_" + project_name + "_" + win.wrapped.name + "(sys::state& state) {\n";
		result += "\t" "auto ptr = std::make_unique<" + project_name + "_" + win.wrapped.name + "_t>();\n";
		result += "\t" "ptr->on_create(state);\n";
		result += "\t" "return ptr;\n";
		result += "}\n";
	}


	// lost code
	result += "// LOST-CODE\n";
	for(auto& s : old_code.found_code) {
		if(s.second.used == false && s.second.text.length() > 0) {
			result += "// BEGIN " + s.first + "\n";
			size_t read_position = 0;
			while(read_position < s.second.text.size()) {
				auto line = analyze_line(s.second.text.c_str(), read_position, s.second.text.size());
				if(line.line.starts_with("//"))
					result += std::string(line.line);
				else
					result += "//" + std::string(line.line);
			}
			result += "// END\n";
		}
		result += old_code.lost_code;
	}

	result += "}\n"; // end namespace
	return result;
}

}
