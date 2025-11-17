#include <vector>
#include "templateproject.hpp"
#include "stools.hpp"

namespace template_project {

void project_to_bytes(project const& p, serialization::out_buffer& buffer) {
	// header info
	buffer.start_section();
	buffer.write(p.svg_directory);
	buffer.finish_section();

	auto& t = p;

	//colors
	buffer.start_section();
	for(auto& c : t.colors) {
		buffer.start_section();
		buffer.write(c.display_name);
		buffer.write(c.r);
		buffer.write(c.g);
		buffer.write(c.b);
		buffer.write(c.a);
		buffer.finish_section();
	}
	buffer.finish_section();

	//icons
	buffer.start_section();
	for(auto& i : t.icons) {
		buffer.start_section();
		buffer.write(i.file_name);
		buffer.finish_section();
	}
	buffer.finish_section();

	//backgrounds
	buffer.start_section();
	for(auto& i : t.backgrounds) {
		buffer.start_section();
		buffer.write(i.file_name);
		buffer.write(i.base_x);
		buffer.write(i.base_y);
		buffer.finish_section();
	}
	buffer.finish_section();

	//labels
	buffer.start_section();
	for(auto& i : t.label_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.primary.bg);
		buffer.write(i.primary.text_color);
		buffer.write(i.primary.font_choice);
		buffer.write(i.primary.font_scale);
		buffer.write(i.primary.h_text_margins);
		buffer.write(i.primary.v_text_margins);
		buffer.write(i.primary.h_text_alignment);
		buffer.write(i.primary.v_text_alignment);
		buffer.finish_section();
	}
	buffer.finish_section();

	//buttons
	buffer.start_section();
	for(auto& i : t.button_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.animate_active_transition);
		buffer.write(i.primary.bg);
		buffer.write(i.primary.text_color);
		buffer.write(i.primary.font_choice);
		buffer.write(i.primary.font_scale);
		buffer.write(i.primary.h_text_margins);
		buffer.write(i.primary.v_text_margins);
		buffer.write(i.primary.h_text_alignment);
		buffer.write(i.primary.v_text_alignment);
		buffer.write(i.active.bg);
		buffer.write(i.active.text_color);
		buffer.write(i.active.font_choice);
		buffer.write(i.active.font_scale);
		buffer.write(i.active.h_text_margins);
		buffer.write(i.active.v_text_margins);
		buffer.write(i.active.h_text_alignment);
		buffer.write(i.active.v_text_alignment);
		buffer.write(i.disabled.bg);
		buffer.write(i.disabled.text_color);
		buffer.write(i.disabled.font_choice);
		buffer.write(i.disabled.font_scale);
		buffer.write(i.disabled.h_text_margins);
		buffer.write(i.disabled.v_text_margins);
		buffer.write(i.disabled.h_text_alignment);
		buffer.write(i.disabled.v_text_alignment);
		buffer.finish_section();
	}
	buffer.finish_section();

	//progress bars
	buffer.start_section();
	for(auto& i : t.progress_bar_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.bg_a);
		buffer.write(i.bg_b);
		buffer.write(i.text_color);
		buffer.write(i.font_choice);
		buffer.write(i.h_text_margins);
		buffer.write(i.v_text_margins);
		buffer.write(i.h_text_alignment);
		buffer.write(i.v_text_alignment);
		buffer.write(i.display_percentage_text);
		buffer.finish_section();
	}
	buffer.finish_section();

	//windows
	buffer.start_section();
	for(auto& i : t.window_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.bg);
		buffer.write(i.layout_region_definition);
		buffer.write(i.close_button_definition);
		buffer.write(i.close_button_icon);
		buffer.write(i.h_close_button_margin);
		buffer.write(i.v_close_button_margin);
		buffer.finish_section();
	}
	buffer.finish_section();

	//iconic buttons
	buffer.start_section();
	for(auto& i : t.iconic_button_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.animate_active_transition);
		buffer.write(i.primary.bg);
		buffer.write(i.primary.icon_color);
		buffer.write(i.primary.icon_top);
		buffer.write(i.primary.icon_left);
		buffer.write(i.primary.icon_bottom);
		buffer.write(i.primary.icon_right);
		buffer.write(i.active.bg);
		buffer.write(i.active.icon_color);
		buffer.write(i.active.icon_top);
		buffer.write(i.active.icon_left);
		buffer.write(i.active.icon_bottom);
		buffer.write(i.active.icon_right);
		buffer.write(i.disabled.bg);
		buffer.write(i.disabled.icon_color);
		buffer.write(i.disabled.icon_top);
		buffer.write(i.disabled.icon_left);
		buffer.write(i.disabled.icon_bottom);
		buffer.write(i.disabled.icon_right);
		buffer.finish_section();
	}
	buffer.finish_section();

	//layout regions
	buffer.start_section();
	for(auto& i : t.layout_region_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.page_number_text.bg);
		buffer.write(i.page_number_text.text_color);
		buffer.write(i.page_number_text.font_choice);
		buffer.write(i.page_number_text.font_scale);
		buffer.write(i.page_number_text.h_text_margins);
		buffer.write(i.page_number_text.v_text_margins);
		buffer.write(i.page_number_text.h_text_alignment);
		buffer.write(i.page_number_text.v_text_alignment);
		buffer.write(i.bg);
		buffer.write(i.left_button);
		buffer.write(i.left_button_icon);
		buffer.write(i.right_button);
		buffer.write(i.right_button_icon);
		buffer.finish_section();
	}
	buffer.finish_section();

	//mixed buttons
	buffer.start_section();
	for(auto& i : t.mixed_button_t) {
		buffer.start_section();
		buffer.write(i.display_name);
		buffer.write(i.primary.bg);
		buffer.write(i.primary.shared_color);
		buffer.write(i.primary.font_choice);
		buffer.write(i.primary.font_scale);
		buffer.write(i.primary.h_text_margins);
		buffer.write(i.primary.v_text_margins);
		buffer.write(i.primary.h_text_alignment);
		buffer.write(i.primary.v_text_alignment);
		buffer.write(i.primary.icon_top);
		buffer.write(i.primary.icon_left);
		buffer.write(i.primary.icon_bottom);
		buffer.write(i.primary.icon_right);

		buffer.write(i.active.bg);
		buffer.write(i.active.shared_color);
		buffer.write(i.active.font_choice);
		buffer.write(i.active.font_scale);
		buffer.write(i.active.h_text_margins);
		buffer.write(i.active.v_text_margins);
		buffer.write(i.active.h_text_alignment);
		buffer.write(i.active.v_text_alignment);
		buffer.write(i.active.icon_top);
		buffer.write(i.active.icon_left);
		buffer.write(i.active.icon_bottom);
		buffer.write(i.active.icon_right);

		buffer.write(i.disabled.bg);
		buffer.write(i.disabled.shared_color);
		buffer.write(i.disabled.font_choice);
		buffer.write(i.disabled.font_scale);
		buffer.write(i.disabled.h_text_margins);
		buffer.write(i.disabled.v_text_margins);
		buffer.write(i.disabled.h_text_alignment);
		buffer.write(i.disabled.v_text_alignment);
		buffer.write(i.disabled.icon_top);
		buffer.write(i.disabled.icon_left);
		buffer.write(i.disabled.icon_bottom);
		buffer.write(i.disabled.icon_right);

		buffer.write(i.animate_active_transition);
		buffer.finish_section();
	}
	buffer.finish_section();

}

project bytes_to_project(serialization::in_buffer& buffer) {
	project result;
	auto header_section = buffer.read_section();
	header_section.read(result.svg_directory);


		auto colors_section = buffer.read_section();
		while(colors_section) {
			result.colors.emplace_back();
			auto individual_color = colors_section.read_section();
			individual_color.read(result.colors.back().display_name);
			individual_color.read(result.colors.back().r);
			individual_color.read(result.colors.back().g);
			individual_color.read(result.colors.back().b);
			individual_color.read(result.colors.back().a);
		}

		auto icons_section = buffer.read_section();
		while(icons_section) {
			result.icons.emplace_back();
			auto indv_icon = icons_section.read_section();
			indv_icon.read(result.icons.back().file_name);
		}

		auto bg_section = buffer.read_section();
		while(bg_section) {
			result.backgrounds.emplace_back();
			auto indv_icon = bg_section.read_section();
			indv_icon.read(result.backgrounds.back().file_name);
			indv_icon.read(result.backgrounds.back().base_x);
			indv_icon.read(result.backgrounds.back().base_y);
		}

		auto labels_section = buffer.read_section();
		while(labels_section) {
			result.label_t.emplace_back();
			auto indv_label = labels_section.read_section();
			indv_label.read(result.label_t.back().display_name);
			indv_label.read(result.label_t.back().primary.bg);
			indv_label.read(result.label_t.back().primary.text_color);
			indv_label.read(result.label_t.back().primary.font_choice);
			indv_label.read(result.label_t.back().primary.font_scale);
			indv_label.read(result.label_t.back().primary.h_text_margins);
			indv_label.read(result.label_t.back().primary.v_text_margins);
			indv_label.read(result.label_t.back().primary.h_text_alignment);
			indv_label.read(result.label_t.back().primary.v_text_alignment);
		}

		auto buttons_section = buffer.read_section();
		while(buttons_section) {
			result.button_t.emplace_back();
			auto indv_button = buttons_section.read_section();
			indv_button.read(result.button_t.back().display_name);
			indv_button.read(result.button_t.back().animate_active_transition);
			indv_button.read(result.button_t.back().primary.bg);
			indv_button.read(result.button_t.back().primary.text_color);
			indv_button.read(result.button_t.back().primary.font_choice);
			indv_button.read(result.button_t.back().primary.font_scale);
			indv_button.read(result.button_t.back().primary.h_text_margins);
			indv_button.read(result.button_t.back().primary.v_text_margins);
			indv_button.read(result.button_t.back().primary.h_text_alignment);
			indv_button.read(result.button_t.back().primary.v_text_alignment);
			indv_button.read(result.button_t.back().active.bg);
			indv_button.read(result.button_t.back().active.text_color);
			indv_button.read(result.button_t.back().active.font_choice);
			indv_button.read(result.button_t.back().active.font_scale);
			indv_button.read(result.button_t.back().active.h_text_margins);
			indv_button.read(result.button_t.back().active.v_text_margins);
			indv_button.read(result.button_t.back().active.h_text_alignment);
			indv_button.read(result.button_t.back().active.v_text_alignment);
			indv_button.read(result.button_t.back().disabled.bg);
			indv_button.read(result.button_t.back().disabled.text_color);
			indv_button.read(result.button_t.back().disabled.font_choice);
			indv_button.read(result.button_t.back().disabled.font_scale);
			indv_button.read(result.button_t.back().disabled.h_text_margins);
			indv_button.read(result.button_t.back().disabled.v_text_margins);
			indv_button.read(result.button_t.back().disabled.h_text_alignment);
			indv_button.read(result.button_t.back().disabled.v_text_alignment);
		}

		auto pbs_section = buffer.read_section();
		while(pbs_section) {
			result.progress_bar_t.emplace_back();
			auto indv_pb = pbs_section.read_section();
			indv_pb.read(result.progress_bar_t.back().display_name);
			indv_pb.read(result.progress_bar_t.back().bg_a);
			indv_pb.read(result.progress_bar_t.back().bg_b);
			indv_pb.read(result.progress_bar_t.back().text_color);
			indv_pb.read(result.progress_bar_t.back().font_choice);
			indv_pb.read(result.progress_bar_t.back().h_text_margins);
			indv_pb.read(result.progress_bar_t.back().v_text_margins);
			indv_pb.read(result.progress_bar_t.back().h_text_alignment);
			indv_pb.read(result.progress_bar_t.back().v_text_alignment);
			indv_pb.read(result.progress_bar_t.back().display_percentage_text);
		}

		auto windows_section = buffer.read_section();
		while(windows_section) {
			result.window_t.emplace_back();
			auto indv_win = windows_section.read_section();
			indv_win.read(result.window_t.back().display_name);
			indv_win.read(result.window_t.back().bg);
			indv_win.read(result.window_t.back().layout_region_definition);
			indv_win.read(result.window_t.back().close_button_definition);
			indv_win.read(result.window_t.back().close_button_icon);
			indv_win.read(result.window_t.back().h_close_button_margin);
			indv_win.read(result.window_t.back().v_close_button_margin);
		}

		auto ibuttons_section = buffer.read_section();
		while(ibuttons_section) {
			result.iconic_button_t.emplace_back();
			auto indv_ibutton = ibuttons_section.read_section();
			indv_ibutton.read(result.iconic_button_t.back().display_name);
			indv_ibutton.read(result.iconic_button_t.back().animate_active_transition);
			indv_ibutton.read(result.iconic_button_t.back().primary.bg);
			indv_ibutton.read(result.iconic_button_t.back().primary.icon_color);
			indv_ibutton.read(result.iconic_button_t.back().primary.icon_top);
			indv_ibutton.read(result.iconic_button_t.back().primary.icon_left);
			indv_ibutton.read(result.iconic_button_t.back().primary.icon_bottom);
			indv_ibutton.read(result.iconic_button_t.back().primary.icon_right);
			indv_ibutton.read(result.iconic_button_t.back().active.bg);
			indv_ibutton.read(result.iconic_button_t.back().active.icon_color);
			indv_ibutton.read(result.iconic_button_t.back().active.icon_top);
			indv_ibutton.read(result.iconic_button_t.back().active.icon_left);
			indv_ibutton.read(result.iconic_button_t.back().active.icon_bottom);
			indv_ibutton.read(result.iconic_button_t.back().active.icon_right);
			indv_ibutton.read(result.iconic_button_t.back().disabled.bg);
			indv_ibutton.read(result.iconic_button_t.back().disabled.icon_color);
			indv_ibutton.read(result.iconic_button_t.back().disabled.icon_top);
			indv_ibutton.read(result.iconic_button_t.back().disabled.icon_left);
			indv_ibutton.read(result.iconic_button_t.back().disabled.icon_bottom);
			indv_ibutton.read(result.iconic_button_t.back().disabled.icon_right);
		}

		auto layout_regions_section = buffer.read_section();
		while(layout_regions_section) {
			result.layout_region_t.emplace_back();
			auto indv_lr = layout_regions_section.read_section();
			indv_lr.read(result.layout_region_t.back().display_name);
			indv_lr.read(result.layout_region_t.back().page_number_text.bg);
			indv_lr.read(result.layout_region_t.back().page_number_text.text_color);
			indv_lr.read(result.layout_region_t.back().page_number_text.font_choice);
			indv_lr.read(result.layout_region_t.back().page_number_text.font_scale);
			indv_lr.read(result.layout_region_t.back().page_number_text.h_text_margins);
			indv_lr.read(result.layout_region_t.back().page_number_text.v_text_margins);
			indv_lr.read(result.layout_region_t.back().page_number_text.h_text_alignment);
			indv_lr.read(result.layout_region_t.back().page_number_text.v_text_alignment);
			indv_lr.read(result.layout_region_t.back().bg);
			indv_lr.read(result.layout_region_t.back().left_button);
			indv_lr.read(result.layout_region_t.back().left_button_icon);
			indv_lr.read(result.layout_region_t.back().right_button);
			indv_lr.read(result.layout_region_t.back().right_button_icon);
		}

		auto mb_section = buffer.read_section();
		while(mb_section) {
			result.mixed_button_t.emplace_back();
			auto indv_mb = mb_section.read_section();
			auto& i = result.mixed_button_t.back();

			indv_mb.read(i.display_name);

			indv_mb.read(i.primary.bg);
			indv_mb.read(i.primary.shared_color);
			indv_mb.read(i.primary.font_choice);
			indv_mb.read(i.primary.font_scale);
			indv_mb.read(i.primary.h_text_margins);
			indv_mb.read(i.primary.v_text_margins);
			indv_mb.read(i.primary.h_text_alignment);
			indv_mb.read(i.primary.v_text_alignment);
			indv_mb.read(i.primary.icon_top);
			indv_mb.read(i.primary.icon_left);
			indv_mb.read(i.primary.icon_bottom);
			indv_mb.read(i.primary.icon_right);

			indv_mb.read(i.active.bg);
			indv_mb.read(i.active.shared_color);
			indv_mb.read(i.active.font_choice);
			indv_mb.read(i.active.font_scale);
			indv_mb.read(i.active.h_text_margins);
			indv_mb.read(i.active.v_text_margins);
			indv_mb.read(i.active.h_text_alignment);
			indv_mb.read(i.active.v_text_alignment);
			indv_mb.read(i.active.icon_top);
			indv_mb.read(i.active.icon_left);
			indv_mb.read(i.active.icon_bottom);
			indv_mb.read(i.active.icon_right);

			indv_mb.read(i.disabled.bg);
			indv_mb.read(i.disabled.shared_color);
			indv_mb.read(i.disabled.font_choice);
			indv_mb.read(i.disabled.font_scale);
			indv_mb.read(i.disabled.h_text_margins);
			indv_mb.read(i.disabled.v_text_margins);
			indv_mb.read(i.disabled.h_text_alignment);
			indv_mb.read(i.disabled.v_text_alignment);
			indv_mb.read(i.disabled.icon_top);
			indv_mb.read(i.disabled.icon_left);
			indv_mb.read(i.disabled.icon_bottom);
			indv_mb.read(i.disabled.icon_right);

			indv_mb.read(i.animate_active_transition);
		}
	
		auto tb_section = buffer.read_section();
		while(tb_section) {
			result.toggle_button_t.emplace_back();
			auto indv_tb = tb_section.read_section();
			auto& i = result.toggle_button_t.back();

			indv_tb.read(i.display_name);
			indv_tb.read(i.on_region.primary.bg);
			indv_tb.read(i.on_region.primary.color);
			indv_tb.read(i.on_region.active.bg);
			indv_tb.read(i.on_region.active.color);
			indv_tb.read(i.on_region.disabled.bg);
			indv_tb.read(i.on_region.disabled.color);
			indv_tb.read(i.on_region.font_choice);
			indv_tb.read(i.on_region.font_scale);
			indv_tb.read(i.on_region.h_text_alignment);
			indv_tb.read(i.on_region.v_text_alignment);
			indv_tb.read(i.on_region.text_margin_left);
			indv_tb.read(i.on_region.text_margin_right);
			indv_tb.read(i.on_region.text_margin_top);
			indv_tb.read(i.on_region.text_margin_bottom);

			indv_tb.read(i.off_region.primary.bg);
			indv_tb.read(i.off_region.primary.color);
			indv_tb.read(i.off_region.active.bg);
			indv_tb.read(i.off_region.active.color);
			indv_tb.read(i.off_region.disabled.bg);
			indv_tb.read(i.off_region.disabled.color);
			indv_tb.read(i.off_region.font_choice);
			indv_tb.read(i.off_region.font_scale);
			indv_tb.read(i.off_region.h_text_alignment);
			indv_tb.read(i.off_region.v_text_alignment);
			indv_tb.read(i.off_region.text_margin_left);
			indv_tb.read(i.off_region.text_margin_right);
			indv_tb.read(i.off_region.text_margin_top);
			indv_tb.read(i.off_region.text_margin_bottom);

			indv_tb.read(i.animate_active_transition);
		}


		auto tables_section = buffer.read_section();
		while(tables_section) {
			result.table_t.emplace_back();
			auto indv_tb = tables_section.read_section();
			auto& i = result.table_t.back();

			indv_tb.read(i.display_name);
			indv_tb.read(i.arrow_increasing);
			indv_tb.read(i.arrow_decreasing);
			indv_tb.read(i.table_color);
			indv_tb.read(i.interactable_header_bg);
			indv_tb.read(i.active_header_bg);
		}

	return result;
}

}

