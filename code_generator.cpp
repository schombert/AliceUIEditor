#include "code_generator.hpp"
#include <string_view>
#include "filesystem.hpp"

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

std::string generate_project_code(open_project_t& proj, code_snippets& old_code) {
	// fix parents
	for(auto& win : proj.windows) {
		for(auto& c : win.children) {
			if(c.container_type != container_type::none && c.child_window.size() > 0) {
				for(auto& winb : proj.windows) {
					if(winb.wrapped.name == c.child_window) {
						winb.wrapped.parent = win.wrapped.name;
					}
				}
			}
		}
	}
	std::string result;
	result += "namespace alice_ui {\n";

	auto project_name = fs::native_to_utf8(proj.project_name);
	// type declarations

	for(auto& win : proj.windows) {
		for(auto& c : win.children) {
			result += "struct " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t : public " + (c.container_type == container_type::none ? "ui::element_base"  : "ui::container_base" ) + " {\n";

			for(auto& dm : c.members) {
				result += "\t" + dm.type + " " + dm.name + ";\n";
			}

			if(c.container_type != container_type::none) {
				result += "\t" "int32_t page = 0;\n";
				result += "\t" "std::vector<" + c.list_content + "> values;\n";
				result += "\t" "std::vector<ui::element_base*> visible_items;\n";
			}

			if(c.background == background_type::existing_gfx) {
				result += "\t"  "std::string_view gfx_key;\n";
				result += "\t"  "dcon::gfx_object_id background_gid;\n";
				result += "\t"  "int32_t frame = 0;\n";
			} else if(c.background != background_type::none) {
				result += "\t"  "std::string_view texture_key;\n";
				if(c.has_alternate_bg) {
					result += "\t"  "std::string_view alt_texture_key;\n";
					result += "\t"  "dcon::texture_id alt_background_texture;\n";
					result += "\t"  "bool is_active = false;\n";
				}
				result += "\t"  "dcon::texture_id background_texture;\n";
				if(c.background == background_type::bordered_texture) {
					result += "\t"  "int16_t border_size = 0;\n";
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
				result += "\t" "\t" "return ui::tooltip_behavior::variable_tooltip;\n";
			} else if(c.tooltip_text_key.length() > 0) {
				result += "\t" "\t" "return ui::tooltip_behavior::tooltip;\n";
			} else {
				result += "\t" "\t" "return ui::tooltip_behavior::no_tooltip;\n";
			}
			result += "\t" "}\n";

			result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
			result += "\t" "\t" "if(type == ui::mouse_probe_type::click) {\n";
			if(c.background != background_type::none) {
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
			} else {
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
			}
			result += "\t" "\t" "} else if(type == ui::mouse_probe_type::tooltip) {\n";
			if(c.dynamic_tooltip || c.tooltip_text_key.length() > 0) {
				result += "\t" "\t" "\t" "return ui::message_result::consumed;\n";
			} else {
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
			}
			result += "\t" "\t" "} else if(type == ui::mouse_probe_type::scroll) {\n";
			if(c.container_type != container_type::none) {
				result += "\t" "\t" "\t" "return (values.size() > visible_items.size()) ? ui::message_result::consumed : ui::message_result::unseen;\n";
			} else {
				result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
			}
			result += "\t" "\t" "} else {\n";
			result += "\t" "\t" "\t" "return ui::message_result::unseen;\n";
			result += "\t" "\t" "}\n";
			result += "\t" "}\n";

			if(c.background != background_type::none) {
				result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
				result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
			}
			if(c.dynamic_tooltip || c.tooltip_text_key.length() > 0) {
				result += "\t"  "void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override;\n";
			}
			if(c.container_type != container_type::none) {
				result += "\t" "ui::message_result on_scroll(sys::state & state, int32_t x, int32_t y, float amount, sys::key_modifiers mods) noexcept override; \n";
				result += "\t" "void change_page(sys::state & state, int32_t new_page); \n";
				result += "\t" "int32_t max_page(); \n";
			}
			result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
			result += "};\n";
		}
	}
	for(auto& win : proj.windows) {
		result += "struct " + project_name + "_" + win.wrapped.name + "_t : public ui::container_base {\n";

		for(auto& dm : win.wrapped.members) {
			result += "\t" + dm.type + " " + dm.name + ";\n";
		}
		for(auto& c : win.children) {
			result += "\t" + project_name + "_" + win.wrapped.name + "_" + c.name + "_t* " + c.name + " = nullptr;\n";
		}

		if(win.wrapped.background == background_type::existing_gfx) {
			result += "\t"  "std::string_view gfx_key;\n";
			result += "\t"  "dcon::gfx_object_id background_gid;\n";
			result += "\t"  "int32_t frame = 0;\n";
		} else if(win.wrapped.background != background_type::none) {
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

		result += "\t" "void on_create(sys::state& state) noexcept override;\n";

		if(win.wrapped.background != background_type::none) {
			result += "\t" "void render(sys::state & state, int32_t x, int32_t y) noexcept override;\n";
			result += "\t"  "ui::message_result on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
			result += "\t"  "ui::message_result on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept override;\n";
		}

		result += "\t" "ui::message_result test_mouse(sys::state& state, int32_t x, int32_t y, ui::mouse_probe_type type) noexcept override {\n";
		if(win.wrapped.background != background_type::none) {
			result += "\t" "\t" "return (type == ui::mouse_probe_type::scroll ? ui::message_result::unseen : ui::message_result::consumed);\n";
		} else {
			result += "\t" "\t" "return ui::message_result::unseen;\n";
		}
		result += "\t" "}\n";
		result += "\t"  "void on_update(sys::state& state) noexcept override;\n";
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
					for(auto& wwin : proj.windows) {
						if(wwin.wrapped.name == w->parent) {
							w = &wwin.wrapped;
							found = true;
							break;
						}
					}
					if(!found)
						w = nullptr;
					if(skip_self) {
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

		for(auto& c : win.children) { // child functions
			// MOUSE
			if(c.background != background_type::none) {
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
				result += "\t" "return ui::message_result::consumed;\n";
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
				result += "\t" "return ui::message_result::consumed;\n";
				result += "}\n";
			}

			// TOOLTIP
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
			} else if(c.tooltip_text_key.length() > 0) {
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept {\n";
				result += "\t" "text::add_line(state, contents, tooltip_key);\n";
				result += "}\n";
			}
			
			// TEXT
			if(c.text_key.length() > 0 || c.dynamic_text) {
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::set_text(sys::state& state, std::string const& new_text) {\n";
				result += "\t" "if(new_text != cached_text) {\n";
				result += "\t" "\t" "cached_text = new_text;\n";
				result += "\t" "\t" "internal_layout.contents.clear();\n";
				result += "\t" "\t" "internal_layout.number_of_lines = 0;\n";
				result += "\t" "\t" "text::single_line_layout sl{ internal_layout, text::layout_parameters{ 0, 0, static_cast<int16_t>(base_data.size.x), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, text_is_header, text_scale * " + std::to_string(2 * proj.grid_size) + "), 0, text_alignment, text::text_color::black, true, true }, state.world.locale_get_native_rtl(state.font_collection.get_current_locale()) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
				result += "\t" "\t" "sl.add_text(state, cached_text);\n";
				result += "\t" "}\n";
				result += "}\n";

				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_reset_text(sys::state& state) noexcept {\n";
				if(!c.dynamic_text) {
					result += "\t" "cached_text = text::produce_simple_string(state, text_key);\n";
					result += "\t" "internal_layout.contents.clear();\n";
					result += "\t" "internal_layout.number_of_lines = 0;\n";
					result += "\t" "text::single_line_layout sl{ internal_layout, text::layout_parameters{ 0, 0, static_cast<int16_t>(base_data.size.x), static_cast<int16_t>(base_data.size.y), text::make_font_id(state, text_is_header, text_scale * " + std::to_string(2 * proj.grid_size) + "), 0, text_alignment, text::text_color::black, true, true }, state.world.locale_get_native_rtl(state.font_collection.get_current_locale()) ? text::layout_base::rtl_status::rtl : text::layout_base::rtl_status::ltr };\n";
					result += "\t" "sl.add_text(state, cached_text);\n";
				}
				result += "}\n";
			}

			// RENDER
			//state.world.locale_get_native_rtl(state.font_collection.get_current_locale())
			if(c.text_key.length() > 0 || c.dynamic_text || c.background != background_type::none) {
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
				if(c.background == background_type::existing_gfx) {
					result += "\t" "if(background_gid) {\n";
					result += "\t""\t" "auto& gfx_def = state.ui_defs.gfx[background_gid];\n";
					result += "\t" "\t" "if(gfx_def.primary_texture_handle) {\n";
					result += "\t"  "\t" "\t" "if(gfx_def.get_object_type() == ui::object_type::bordered_rect) {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), gfx_def.type_dependent, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					result += "\t""\t" "\t" "} else if(gfx_def.number_of_frames > 1) {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), frame, gfx_def.number_of_frames, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					result += "\t" "\t" "\t" "} else {\n";
					result += "\t" "\t" "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					result += "\t" "\t" "\t" "}\n";
					result += "\t" "\t" "}\n";
					result += "\t" "}\n";
				} else if(c.background == background_type::texture) {
					if(c.has_alternate_bg) {
						result += "\t"  "if(is_active)\n";
						result += "\t"   "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
						result += "\t"  "else\n";
						result += "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + ");\n";
					} else {
						result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") +");\n";
					}
				} else if(c.background == background_type::bordered_texture) {
					if(c.has_alternate_bg) {
						result += "\t" "if(is_active)\n";
						result += "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
						result += "\t" "else\n";
						result += "\t"  "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					} else {
						result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(c.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					}
				}

				if(c.text_key.length() > 0 || c.dynamic_text) {
					result += "\t"  "if(internal_layout.contents.empty()) return;\n";
					result += "\t"  "auto fh = text::make_font_id(state, text_is_header, text_scale * " + std::to_string(proj.grid_size * 2) + ");\n";
					result += "\t"  "auto linesz = state.font_collection.line_height(state, fh); \n";
					result += "\t" "if(linesz == 0.0f) return;\n";
					result += "\t"  "auto ycentered = (base_data.size.y - linesz) / 2;\n";
					result += "\t"  "auto cmod = ui::get_color_modification(this == state.ui_state.under_mouse, " + std::string(c.can_disable ? "disabled" : "false") + ", " + std::string((c.left_click_action || c.right_click_action || c.shift_click_action) ? "true" : "false") + "); \n";
					result += "\t"  "for(auto& t : internal_layout.contents) {\n";
					result += "\t"  "\t" "ui::render_text_chunk(state, t, float(x) + t.x, float(y + int32_t(ycentered)),  fh, ui::get_text_color(state, text_color), cmod);\n";
					result += "\t"  "}\n";
				}
				result += "}\n";
			}

			//SCROLL
			if(c.container_type != container_type::none) {
				result += "int32_t " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::max_page(){\n";
				result += "\t" "if(values.size() == 0) return 0;\n";
				result += "\t" "if(visible_items.size() == 0) return 0;\n";
				result += "\t" "return int32_t(values.size() - 1) / int32_t(visible_items.size());\n";
				result += "}\n";
				result += "void " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::change_page(sys::state & state, int32_t new_page) {\n";
				result += "\t" "page = std::clamp(new_page, 0, max_page());\n";
				result += "\t" "state.game_state_updated.store(true, std::memory_order::release);\n";
				result += "}\n";

				result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_" + c.name + "_t::on_scroll(sys::state& state, int32_t x, int32_t y, float amount, sys::key_modifiers mods) noexcept {\n";
				result += "\t""change_page(state, page + ((amount < 0) ? 1 : -1));\n";
				result += "\t" "return ui::message_result::consumed;\n";
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
			if(c.container_type != container_type::none) {
				auto child_type = project_name + "_" + c.child_window + "_t";
				result += "\t" "page = std::clamp(page, 0, max_page());\n";
				result += "\t" "auto fill_count = visible_items.size();\n";
				result += "\t" "auto fill_start = size_t(page) * fill_count;\n";
				result += "\t" "for(size_t fill_position = 0; fill_position < fill_count; ++fill_position) {\n";
				result += "\t" "\t" "if(fill_position + fill_start < values.size()) {\n";
				result += "\t" "\t" "\t" "if( ((" + child_type + "*)visible_items[fill_position])->value != values[fill_position + fill_start] && visible_items[fill_position]->is_visible()) {\n";
				result += "\t" "\t" "\t"  "\t" "((" + child_type + "*)visible_items[fill_position])->value = values[fill_position + fill_start];\n";
				result += "\t" "\t" "\t"  "\t" "visible_items[fill_position]->impl_on_update(state);\n";
				result += "\t" "\t" "\t" "} else {\n";
				result += "\t" "\t" "\t"  "\t" "((" + child_type + "*)visible_items[fill_position])->value = values[fill_position+ fill_start];\n";
				result += "\t" "\t" "\t"  "\t" "visible_items[fill_position]->set_visible(state, true);\n";
				result += "\t" "\t" "\t" "}\n";
				result += "\t" "\t" "} else {\n";
				result += "\t" "\t" "\t" "visible_items[fill_position]->set_visible(state, false);\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";
			}
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
			if(c.container_type == container_type::list) {
				result += "\t" "auto ptr = make_" + project_name + "_" + c.child_window + "(state);\n";
				result += "\t" "int32_t item_y_size = ptr->base_data.size.y;\n";
				result += "\t" "visible_items.push_back(ptr.get());\n";
				result += "\t" "add_child_to_back(std::move(ptr));\n";
				result += "\t" "auto total_rows = int32_t(base_data.size.y) / item_y_size;\n";
				result += "\t" "for(int32_t i = 1; i < total_rows; ++i) {\n";
				result += "\t" "\t" "auto ptrb = make_" + project_name + "_" + c.child_window + "(state);\n";
				result += "\t" "\t" "ptrb->base_data.position.y = int16_t(i * item_y_size);\n";
				result += "\t" "\t" "visible_items.push_back(ptrb.get());\n";
				result += "\t" "\t" "add_child_to_back(std::move(ptrb));\n";
				result += "\t" "}\n";
			} else if(c.container_type == container_type::grid) {
				result += "\t" "auto ptr = make_" + project_name + "_" + c.child_window + "(state);\n";
				result += "\t" "int32_t item_y_size = ptr->base_data.size.y;\n";
				result += "\t" "int32_t item_x_size = ptr->base_data.size.x;\n";
				result += "\t" "visible_items.push_back(ptr.get());\n";
				result += "\t" "add_child_to_back(std::move(ptr));\n";
				result += "\t" "auto total_rows = int32_t(base_data.size.y) / item_y_size;\n";
				result += "\t" "auto total_columns = int32_t(base_data.size.x) / item_x_size;\n";
				result += "\t" "for(int32_t i = 0; i < total_rows; ++i) {\n";
				result += "\t" "\t"  "for(int32_t j = (i == 0 ? 1 : 0); j < total_columns; ++j) {\n";
				result += "\t" "\t" "\t" "auto ptrb = make_" + project_name + "_" + c.child_window + "(state);\n";
				result += "\t" "\t" "\t" "ptrb->base_data.position.y = int16_t(i * item_y_size);\n";
				result += "\t" "\t" "\t" "ptrb->base_data.position.x = int16_t(j * item_x_size);\n";
				result += "\t" "\t" "\t" "visible_items.push_back(ptrb.get());\n";
				result += "\t" "\t" "\t" "add_child_to_back(std::move(ptrb));\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";
			}
			result += "// BEGIN " + win.wrapped.name + "::" + c.name + "::create\n";
			if(auto it = old_code.found_code.find(win.wrapped.name + "::" + c.name + "::create"); it != old_code.found_code.end()) {
				it->second.used = true;
				result += it->second.text;
			}
			result += "// END\n";
			result += "}\n";
		}

		// window functions
		if(win.wrapped.background != background_type::none) {
			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_t::on_lbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
			result += "\t" "return ui::message_result::consumed;\n";
			result += "}\n";

			result += "ui::message_result " + project_name + "_" + win.wrapped.name + "_t::on_rbutton_down(sys::state& state, int32_t x, int32_t y, sys::key_modifiers mods) noexcept {\n";
			result += "\t" "return ui::message_result::consumed;\n";
			result += "}\n";

			result += "void " + project_name + "_" + win.wrapped.name + "_t::render(sys::state & state, int32_t x, int32_t y) noexcept {\n";
			if(win.wrapped.background == background_type::existing_gfx) {
				result += "\t" "if(background_gid) {\n";
				result += "\t" "\t" "auto& gfx_def = state.ui_defs.gfx[background_gid];\n";
				result += "\t" "\t" "if(gfx_def.primary_texture_handle) {\n";
				result += "\t" "\t" "\t" "if(gfx_def.get_object_type() == ui::object_type::bordered_rect) {\n";
				result += "\t" "\t" "\t" "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), gfx_def.type_dependent, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + ");\n";
				result += "\t" "\t" "\t" "} else if(gfx_def.number_of_frames > 1) {\n";
				result += "\t" "\t" "\t" "\t" "ogl::render_subsprite(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), frame, gfx_def.number_of_frames, float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped(), " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + ");\n";
				result += "\t" "\t" "\t" "} else {\n";
				result += "\t" "\t" "\t" "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_texture_handle(state, gfx_def.primary_texture_handle, gfx_def.is_partially_transparent()), base_data.get_rotation(), gfx_def.is_vertically_flipped()," + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + ");\n";
				result += "\t" "\t" "\t" "}\n";
				result += "\t" "\t" "}\n";
				result += "\t" "}\n";
			} else if(win.wrapped.background == background_type::texture) {
				if(win.wrapped.has_alternate_bg) {
					result += "\t" "if(is_active)\n";
					result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					result += "\t" "else\n";
					result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
				} else {
					result += "\t" "ogl::render_textured_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
				}
			} else if(win.wrapped.background == background_type::bordered_texture) {
				if(win.wrapped.has_alternate_bg) {
					result += "\t" "if(is_active)\n";
					result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, alt_background_texture, alt_texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
					result += "\t" "else\n";
					result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
				} else {
					result += "\t" "ogl::render_bordered_rect(state, ui::get_color_modification(this == state.ui_state.under_mouse, false, false), float(border_size), float(x), float(y), float(base_data.size.x), float(base_data.size.y), ogl::get_late_load_texture_handle(state, background_texture, texture_key), base_data.get_rotation(), false, " + std::string(win.wrapped.ignore_rtl ? "false" : "state.world.locale_get_native_rtl(state.font_collection.get_current_locale())") + "); \n";
				}
			}
			
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
		result += "}\n";

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

		if(win.wrapped.background == background_type::existing_gfx) {
			result += "\t"  "gfx_key = win_data.texture;\n";
			result += "\t" "if(auto it = state.ui_state.gfx_by_name.find(state.lookup_key(gfx_key)); it != state.ui_state.gfx_by_name.end()) {\n";
			result += "\t" "\t" "background_gid = it->second;\n";
			result += "\t" "}\n";
		} else if(win.wrapped.background != background_type::none) {
			result += "\t"   "texture_key = win_data.texture;\n";
			if(win.wrapped.background == background_type::bordered_texture) {
				result += "\t"  "\t"  "border_size = win_data.border_size;\n";
			}
			if(win.wrapped.has_alternate_bg) {
				result += "\t"   "alt_texture_key = win_data.alt_texture;\n";
			}
		}

		if(win.wrapped.updates_while_hidden) {
			result += "\t"  "\t"  "ui::element_base::flags |= ui::element_base::wants_update_when_hidden_mask;\n";
		}

		result += "\t" "auto name_key = state.lookup_key(\"" + project_name + "::" + win.wrapped.name + "\");\n";
		result += "\t" "for(auto ex : state.ui_defs.extensions) {\n";
		result += "\t" "\t" "if(name_key && ex.window == name_key) {\n";
		result += "\t" "\t" "\t" "auto ch_res = ui::make_element_immediate(state, ex.child);\n";
		result += "\t" "\t" "\t" "if(ch_res) {\n";
		result += "\t" "\t" "\t" "\t" "this->add_child_to_back(std::move(ch_res));\n";
		result += "\t" "\t" "\t" "}\n";
		result += "\t" "\t" "}\n";
		result += "\t" "}\n";

		result += "\t" "while(!pending_children.empty()) {\n";
		result += "\t" "\t" "auto child_data = read_child_bytes(pending_children.back().data, pending_children.back().size);\n";
		result += "\t" "\t" "pending_children.pop_back();\n";
		for(auto& c : win.children) {
			result += "\t" "\t" "if(child_data.name == \"" + c.name + "\") {\n";
			result += "\t" "\t" "\t" "auto cptr = std::make_unique<" + project_name + "_" + win.wrapped.name + "_" + c.name + "_t>();\n";
			result += "\t" "\t" "\t" "cptr->parent = this;\n";
			result += "\t" "\t" "\t" + c.name + " = cptr.get();\n";

			result += "\t" "\t" "\t" "cptr->base_data.position.x = child_data.x_pos;\n";
			result += "\t" "\t" "\t" "cptr->base_data.position.y = child_data.y_pos;\n";
			result += "\t" "\t" "\t" "cptr->base_data.size.x = child_data.x_size;\n";
			result += "\t" "\t" "\t" "cptr->base_data.size.y = child_data.y_size;\n";

			if(c.updates_while_hidden) {
				result += "\t"  "\t"  "cptr->flags |= ui::element_base::wants_update_when_hidden_mask;\n";
			}

			if(c.background == background_type::existing_gfx) {
				result += "\t" "\t" "\t"  "cptr->gfx_key = child_data.texture;\n";
			} else if(c.background != background_type::none) {
				result += "\t" "\t" "\t"  "cptr->texture_key = child_data.texture;\n";
				if(c.background == background_type::bordered_texture) {
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

			result += "\t" "\t" "\t" "cptr->on_create(state);\n";
			result += "\t" "\t" "\t" "this->add_child_to_back(std::move(cptr));\n";
			result += "\t" "\t" "continue;\n";
			result += "\t" "\t" "}\n";
		}
		result += "\t" "}\n";

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
				result += "//" + std::string(line.line);
			}
			result += s.second.text;
			result += "// END\n";
		}
		result += old_code.lost_code;
	}

	result += "}\n"; // end namespace
	return result;
}

}
