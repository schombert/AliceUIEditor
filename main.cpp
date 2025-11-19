#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdio.h>
#include <variant>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellscalingapi.h>
#define GL_SILENCE_DEPRECATION
#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include "glew.h"
#include <glfw3.h>
#include <string>
#include <string_view>
#include <memory>
#include <cmath>
#include "project_description.hpp"
#include "filesystem.hpp"
#include "imgui_stdlib.h"
#include "stools.hpp"
#include "code_generator.hpp"
#include "templateproject.hpp"

namespace edit_targets {
	inline constexpr int window = 0;
	inline constexpr int control = 1;
	inline constexpr int layout_sublayout = 2;
	inline constexpr int layout_control = 3;
	inline constexpr int layout_glue = 4;
	inline constexpr int layout_generator = 5;
	inline constexpr int layout_texture = 6;
	inline constexpr int layout_window = 7;
};

const char* glue_names[] = { "standard", "at least", "line break", "page break", "don't break" };
const char * texture_layer_names[] {
	"Texture",
	"Bordered texture",
	"Corners",
	"Repeat border"
};
background_type texture_layer_available_types[] {
	background_type::texture,
	background_type::bordered_texture,
	background_type::textured_corners,
	background_type::border_texture_repeat
};

size_t retrieve_texture_layer_type(background_type x) {
	int index = 0;
	for (int j = 0; j < 4; j++) {
		if (texture_layer_available_types[j] == x) {
			index = j;
			break;
		}
	}

	return index;
}

open_project_t bytes_to_project(serialization::in_buffer& buffer);
void project_to_aui_bytes(open_project_t const& p, serialization::out_buffer& buffer);
void project_to_bytes(open_project_t const& p, serialization::out_buffer& buffer);

namespace template_project {
void project_to_bytes(project const& p, serialization::out_buffer& buffer);
project bytes_to_project(serialization::in_buffer& buffer);
}

GLint compile_shader(std::string_view source, GLenum type) {
	GLuint return_value = glCreateShader(type);

	if(return_value == 0) {
		MessageBoxW(nullptr, L"shader creation failed", L"OpenGL error", MB_OK);
	}

	std::string s_source(source);
	GLchar const* texts[] = {
		"#version 140\r\n",
		"#extension GL_ARB_explicit_uniform_location : enable\r\n",
		"#extension GL_ARB_explicit_attrib_location : enable\r\n",
		"#extension GL_ARB_shader_subroutine : enable\r\n",
		"#define M_PI 3.1415926535897932384626433832795\r\n",
		"#define PI 3.1415926535897932384626433832795\r\n",
		s_source.c_str()
	};
	glShaderSource(return_value, 7, texts, nullptr);
	glCompileShader(return_value);

	GLint result;
	glGetShaderiv(return_value, GL_COMPILE_STATUS, &result);
	if(result == GL_FALSE) {
		GLint log_length = 0;
		glGetShaderiv(return_value, GL_INFO_LOG_LENGTH, &log_length);

		auto log = std::unique_ptr<char[]>(new char[static_cast<size_t>(log_length)]);
		GLsizei written = 0;
		glGetShaderInfoLog(return_value, log_length, &written, log.get());
		auto error = std::string("Shader failed to compile:\n") + log.get();
		MessageBoxA(nullptr, error.c_str(), "OpenGL error", MB_OK);
	}
	return return_value;
}

GLuint create_program(std::string_view vertex_shader, std::string_view fragment_shader) {
	GLuint return_value = glCreateProgram();
	if(return_value == 0) {
		MessageBoxW(nullptr, L"program creation failed", L"OpenGL error", MB_OK);
	}

	auto v_shader = compile_shader(vertex_shader, GL_VERTEX_SHADER);
	auto f_shader = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

	glAttachShader(return_value, v_shader);
	glAttachShader(return_value, f_shader);
	glLinkProgram(return_value);

	GLint result;
	glGetProgramiv(return_value, GL_LINK_STATUS, &result);
	if(result == GL_FALSE) {
		GLint logLen;
		glGetProgramiv(return_value, GL_INFO_LOG_LENGTH, &logLen);

		char* log = new char[static_cast<size_t>(logLen)];
		GLsizei written;
		glGetProgramInfoLog(return_value, logLen, &written, log);
		auto err = std::string("Program failed to link:\n") + log;
		MessageBoxA(nullptr, err.c_str(), "OpenGL error", MB_OK);
	}

	glDeleteShader(v_shader);
	glDeleteShader(f_shader);

	return return_value;
}

static GLfloat global_square_data[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
static GLfloat global_square_right_data[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
static GLfloat global_square_left_data[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };
static GLfloat global_square_flipped_data[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
static GLfloat global_square_right_flipped_data[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
static GLfloat global_square_left_flipped_data[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

static GLuint ui_shader_program = 0;

void load_shaders() {

	std::string_view fx_str =
		"in vec2 tex_coord;\n"
		"out vec4 frag_color;\n"
		"uniform sampler2D texture_sampler;\n"
		"uniform vec4 d_rect;\n"
		"uniform float border_size;\n"
		"uniform float grid_size;\n"
		"uniform vec2 grid_off;\n"
		"uniform vec3 inner_color;\n"
		"uniform uvec2 subroutines_index;\n"

		"vec4 empty_rect(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.25f);\n"
		"}\n"
		"vec4 hollow_rect(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 4.5 || realy <= 4.5 || realx >= (d_rect.z -4.5) || realy >= (d_rect.w -4.5))\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.0f);\n"
		"}\n"
		"vec4 grid_texture(vec2 tc) {\n"
			"float realx = grid_off.x + tc.x * d_rect.z;\n"
			"float realy = grid_off.y + tc.y * d_rect.w;\n"
			"if(mod(realx, grid_size) < 1.0f || mod(realy, grid_size) < 1.0f)\n"
				"return vec4(1.0f, 1.0f, 1.0f, 0.1f);\n"
			"return vec4(0.0f, 0.0f, 0.0f, 0.0f);\n"
		"}\n"
		"vec4 direct_texture(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"\treturn texture(texture_sampler, tc);\n"
		"}\n"
		"vec4 frame_stretch(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"vec2 tsize = textureSize(texture_sampler, 0);\n"
			"float xout = 0.0;\n"
			"float yout = 0.0;\n"
			"if(realx <= border_size * grid_size)\n"
				"xout = realx / (tsize.x * grid_size);\n"
			"else if(realx >= (d_rect.z - border_size * grid_size))\n"
				"xout = (1.0 - border_size / tsize.x) + (border_size * grid_size - (d_rect.z - realx)) / (tsize.x * grid_size);\n"
			"else\n"
				"xout = border_size / tsize.x + (1.0 - 2.0 * border_size / tsize.x) * (realx - border_size * grid_size) / (d_rect.z * 2.0 * border_size * grid_size);\n"
			"if(realy <= border_size * grid_size)\n"
				"yout = realy / (tsize.y * grid_size);\n"
			"else if(realy >= (d_rect.w - border_size * grid_size))\n"
				"yout = (1.0 - border_size / tsize.y) + (border_size * grid_size - (d_rect.w - realy)) / (tsize.y * grid_size);\n"
			"else\n"
				"yout = border_size / tsize.y + (1.0 - 2.0 * border_size / tsize.y) * (realy - border_size * grid_size) / (d_rect.w * 2.0 * border_size * grid_size);\n"
			"return texture(texture_sampler, vec2(xout, yout));\n"
		"}\n"
		"vec4 coloring_function(vec2 tc) {\n"
			"\tswitch(int(subroutines_index.x)) {\n"
				"\tcase 1: return empty_rect(tc);\n"
				"\tcase 2: return direct_texture(tc);\n"
				"\tcase 3: return frame_stretch(tc);\n"
				"\tcase 4: return grid_texture(tc);\n"
				"\tcase 5: return hollow_rect(tc);\n"
				"\tdefault: break;\n"
			"\t}\n"
			"\treturn vec4(1.0f,1.0f,1.0f,1.0f);\n"
		"}\n"
		"void main() {\n"
			"\tfrag_color = coloring_function(tex_coord);\n"
		"}";
	std::string_view vx_str =
		"layout (location = 0) in vec2 vertex_position;\n"
		"layout (location = 1) in vec2 v_tex_coord;\n"
		"out vec2 tex_coord;\n"
		"uniform float screen_width;\n"
		"uniform float screen_height;\n"
		"uniform vec4 d_rect;\n"
		"void main() {\n"
			"\tgl_Position = vec4(\n"
				"\t\t-1.0 + (2.0 * ((vertex_position.x * d_rect.z)  + d_rect.x) / screen_width),\n"
				"\t\t 1.0 - (2.0 * ((vertex_position.y * d_rect.w)  + d_rect.y) / screen_height),\n"
				"\t\t0.0, 1.0);\n"
			"\ttex_coord = v_tex_coord;\n"
		"}";

	ui_shader_program = create_program(vx_str, fx_str);
}

static GLuint global_square_vao = 0;
static GLuint global_square_buffer = 0;
static GLuint global_square_right_buffer = 0;
static GLuint global_square_left_buffer = 0;
static GLuint global_square_flipped_buffer = 0;
static GLuint global_square_right_flipped_buffer = 0;
static GLuint global_square_left_flipped_buffer = 0;

static GLuint sub_square_buffers[64] = { 0 };

void load_global_squares() {
	glGenBuffers(1, &global_square_buffer);

	// Populate the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, global_square_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_data, GL_STATIC_DRAW);

	glGenVertexArrays(1, &global_square_vao);
	glBindVertexArray(global_square_vao);
	glEnableVertexAttribArray(0); // position
	glEnableVertexAttribArray(1); // texture coordinates

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);					 // position
	glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);	// texture coordinates
	glVertexAttribBinding(0, 0);											// position -> to array zero
	glVertexAttribBinding(1, 0);											 // texture coordinates -> to array zero

	glGenBuffers(1, &global_square_left_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_left_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_left_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_right_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_right_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_right_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_right_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_right_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_right_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_left_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_left_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_left_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(64, sub_square_buffers);
	for(uint32_t i = 0; i < 64; ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, sub_square_buffers[i]);

		float const cell_x = static_cast<float>(i & 7) / 8.0f;
		float const cell_y = static_cast<float>((i >> 3) & 7) / 8.0f;

		GLfloat global_sub_square_data[] = { 0.0f, 0.0f, cell_x, cell_y, 0.0f, 1.0f, cell_x, cell_y + 1.0f / 8.0f, 1.0f, 1.0f,
			cell_x + 1.0f / 8.0f, cell_y + 1.0f / 8.0f, 1.0f, 0.0f, cell_x + 1.0f / 8.0f, cell_y };

		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_sub_square_data, GL_STATIC_DRAW);
	}
}


void render_textured_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight, GLuint texture_handle) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	GLuint subroutines[2] = { 2, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_stretch_textured_rect(color3f color, float ix, float iy, float ui_scale, int32_t iwidth, int32_t iheight, float border_size, GLuint texture_handle) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform1f(glGetUniformLocation(ui_shader_program, "border_size"), border_size);
	glUniform1f(glGetUniformLocation(ui_shader_program, "grid_size"), ui_scale);
	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	GLuint subroutines[2] = { 3, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_empty_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	GLuint subroutines[2] = { 1, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_hollow_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	GLuint subroutines[2] = { 5, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void render_layout_rect(color3f outline_color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	render_empty_rect(outline_color * 0.5f, ix, iy, iwidth, iheight);
	render_hollow_rect(outline_color, ix, iy, iwidth, iheight);
}

static void glfw_error_callback(int error, const char* description) {
	MessageBoxW(nullptr, L"GLFW Error", L"OpenGL error", MB_OK);
}

enum class drag_target {
	none, center, left, right, top, bottom, top_left, top_right, bottom_left, bottom_right
};


std::wstring relative_file_name(std::wstring_view base_name, std::wstring_view project_directory) {
	if(base_name.length() > 1) {
		size_t common_length = 0;
		while(common_length < base_name.size()) {
			auto next_common_length = base_name.find_first_of(L'\\', common_length);
			if (next_common_length != std::string::npos) {
				next_common_length++;
			}
			if(base_name.substr(0, next_common_length) != project_directory.substr(0, next_common_length)) {
				break;
			}
			common_length = next_common_length;
		}
		uint32_t missing_separators_count = 0;
		for(size_t i = common_length; i < project_directory.size(); ++i) {
			if(project_directory[i] == L'\\') {
				++missing_separators_count;
			}
		}
		if(missing_separators_count == 0) {
			if(common_length >= base_name.size()) {
				auto last_sep = base_name.find_last_of(L'\\');
				if(last_sep == std::wstring::npos)
					return std::wstring(base_name);

				return std::wstring(base_name.substr(last_sep + 1));
			} else {
				return std::wstring(base_name.substr(common_length));
			}
		} else {
			std::wstring temp;
			for(uint32_t i = 0; i < missing_separators_count; ++i) {
				temp += L"..\\";
			}
			if(common_length >= base_name.size()) {
				std::abort(); // impossible
				//return temp;
			} else {
				return temp + std::wstring(base_name.substr(common_length));
			}
		}
	}
	return std::wstring(base_name);
}
drag_target test_rect_target(float pos_x, float pos_y, float rx, float ry, float rw, float rh, float scale) {
	bool top = false;
	bool bottom = false;
	bool left = false;
	bool right = false;
	if(rx - 2.0f * scale <= pos_x && pos_x <= rx + 2.0f * scale && ry - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale)
		left = true;
	if(ry - 2.0f * scale <= pos_y && pos_y <= ry + 2.0f * scale && rx - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale)
		top = true;
	if(rx + rw - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale && ry - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale)
		right = true;
	if(ry + rh - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale && rx  - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale)
		bottom = true;
	if(top && bottom)
		return drag_target::bottom;
	if(left && right)
		return drag_target::right;
	if(top && left)
		return drag_target::top_left;
	if(top && right)
		return drag_target::top_right;
	if(bottom && left)
		return drag_target::bottom_left;
	if(bottom && right)
		return drag_target::bottom_right;
	if(left)
		return drag_target::left;
	if(right)
		return drag_target::right;
	if(top)
		return drag_target::top;
	if(bottom)
		return drag_target::bottom;
	if(rx <= pos_x && pos_x <= rx + rw && ry <= pos_y && pos_y <= ry + rh)
		return drag_target::center;
	return drag_target::none;
}

void mouse_to_drag_type(drag_target t) {
	switch(t) {
		case drag_target::none: break;
		case drag_target::center: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeAll); break;
		case drag_target::left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeEW); break;
		case drag_target::right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeEW); break;
		case drag_target::top: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNS); break;
		case drag_target::bottom: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNS); break;
		case drag_target::top_left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNWSE); break;
		case drag_target::top_right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNESW); break;
		case drag_target::bottom_left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNESW); break;
		case drag_target::bottom_right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNWSE); break;
		default: break;
	}

}

float last_scroll_value = 0.0f;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	last_scroll_value += float(yoffset);
}

open_project_t open_project;
template_project::project open_templates;

float drag_offset_x = 0.0f;
float drag_offset_y = 0.0f;
int32_t selected_window = -1;
int32_t chosen_window = -1;
bool just_chose_window = false;
int32_t selected_control = -1;
int32_t hovered_control = -1;
int32_t selected_table = -1;
// layout_level_t* selected_layout = nullptr;
std::vector<size_t> path_to_selected_layout {};
int current_edit_target = edit_targets::window;

bool paths_are_the_same(std::vector<size_t>& left, std::vector<size_t>& right) {
	if (left.size() != right.size()) return false;
	for (size_t i = 0; i < left.size(); i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	return true;
}

template<size_t SIZE>
bool paths_are_the_same(std::array<size_t, SIZE>& left, size_t left_size, std::array<size_t, SIZE>& right, size_t right_size) {
	if (left_size != right_size) return false;
	for (size_t i = 0; i < left_size; i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	return true;
}

bool paths_are_the_same(std::vector<size_t>& left, size_t left_attachment, std::vector<size_t>& right) {
	if (left.size() + 1 != right.size()) return false;
	for (size_t i = 0; i < left.size(); i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	if (left_attachment != right.back()) {
		return false;
	}
	return true;
}

layout_level_t* retrieve_layout(layout_level_t& root, std::vector<size_t>& dir) {
	auto& win = open_project.windows[selected_window];
	layout_level_t* selected_layout = &win.layout;
	for (auto i : dir) {
		selected_layout = std::get<sub_layout_t>(selected_layout->contents[i]).layout.get();
	}

	return selected_layout;
}

template<size_t SIZE>
layout_level_t* retrieve_layout(layout_level_t& root, std::array<size_t, SIZE>& dir, size_t len) {
	auto& win = open_project.windows[selected_window];
	layout_level_t* selected_layout = &win.layout;
	for (auto j = 0; j < len; j++) {

		selected_layout = std::get<sub_layout_t>(selected_layout->contents[dir[j]]).layout.get();
	}
	return selected_layout;
}

void update_cached_control(std::string_view name, window_element_wrapper_t& window, int16_t& index) {
	bool update = false;
	if(index < 0 || int16_t(window.children.size()) <= index)
		update = true;
	if(!update && window.children[index].name != name)
		update = true;
	if(update) {
		for(auto i = window.children.size(); i-- > 0; ) {
			if(window.children[i].name == name) {
				index = int16_t(i);
				return;
			}
		}
		index = int16_t(-1);
	}
}
void update_cached_window(std::string_view name, int16_t& index) {
	bool update = false;
	if(index < 0 || int16_t(open_project.windows.size()) <= index)
		update = true;
	if(!update && open_project.windows[index].wrapped.name != name)
		update = true;
	if(update) {
		for(auto i = open_project.windows.size(); i-- > 0; ) {
			if(open_project.windows[i].wrapped.name == name) {
				index = int16_t(i);
				return;
			}
		}
		index = int16_t(-1);
	}
}

void make_color_combo_box(int32_t& color_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&color_choice);

	int32_t combo_selection = color_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.colors) {
		options.push_back(c.display_name.c_str());
	}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		color_choice = combo_selection - 1;
	}
	ImGui::PopID();
}

void render_layout(window_element_wrapper_t& window, layout_level_t& layout, int layer, float x, float y, int32_t width, int32_t height, color3f outline_color, float scale);

void render_window(window_element_wrapper_t& win, float x, float y, bool highlightwin, float ui_scale) {
	// bg
	if(win.wrapped.template_id != -1) {
		auto render_asvg_rect = [&](asvg::svg& s, float hcursor, float vcursor, float x_sz, float y_sz, int32_t gsz) {
			render_hollow_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f),
				hcursor,
				vcursor,
				std::max(1, int32_t(x_sz * ui_scale)),
				std::max(1, int32_t(y_sz * ui_scale)));

			render_textured_rect(color3f{ 0.f, 0.f, 0.f },
				hcursor,
				vcursor,
				std::max(1, int32_t(x_sz * ui_scale)),
				std::max(1, int32_t(y_sz * ui_scale)),
				s.get_render(x_sz / float(gsz), y_sz / float(gsz), gsz, 2.0f));

		};
		auto render_svg_rect = [&](asvg::simple_svg& s, float hcursor, float vcursor, int32_t x_sz, int32_t y_sz, color3f c) {
			render_textured_rect(color3f{ 0.f, 0.f, 0.f },
				hcursor,
				vcursor,
				std::max(1, int32_t(x_sz * ui_scale)),
				std::max(1, int32_t(y_sz * ui_scale)),
				s.get_render(x_sz, y_sz, 2.0f, c.r, c.g, c.b));
		};

		auto& thm = open_templates;
		auto selected_template = win.wrapped.template_id;

		if(thm.window_t[selected_template].bg != -1) {
			render_asvg_rect(thm.backgrounds[thm.window_t[selected_template].bg].renders, x * ui_scale, y * ui_scale, win.wrapped.x_size, win.wrapped.y_size, open_project.grid_size);
		} else {
			render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
		}
		auto vcursor = (y + thm.window_t[selected_template].v_close_button_margin * open_project.grid_size) * ui_scale;
		auto hcursor = (x+ win.wrapped.x_size - thm.window_t[selected_template].h_close_button_margin * open_project.grid_size - open_project.grid_size * 3.0f) * ui_scale;
		if(win.wrapped.auto_close_button && thm.window_t[selected_template].close_button_definition != -1) {
			auto l = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_left.resolve(float(3 * open_project.grid_size), float(3 * open_project.grid_size), open_project.grid_size) * ui_scale + hcursor;
			auto t = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_top.resolve(float(3 * open_project.grid_size), float(3 * open_project.grid_size), open_project.grid_size) * ui_scale + vcursor;
			auto r = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_right.resolve(float(3 * open_project.grid_size), float(3 * open_project.grid_size), open_project.grid_size) * ui_scale + hcursor;
			auto b = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_bottom.resolve(float(3 * open_project.grid_size), float(3 * open_project.grid_size), open_project.grid_size) * ui_scale + vcursor;

			if(thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.bg != -1)
				render_asvg_rect(thm.backgrounds[thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.bg].renders, hcursor, vcursor, 3 * open_project.grid_size, 3 * open_project.grid_size, open_project.grid_size);

			hcursor = l;
			vcursor = t;

			if(thm.window_t[selected_template].close_button_icon != -1) {
				render_svg_rect(thm.icons[thm.window_t[selected_template].close_button_icon].renders,
					hcursor, vcursor, int32_t((r - l) / ui_scale), int32_t((b - t) / ui_scale),
					thm.colors[thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_color]);
			}
		}
	} else {
		if(
			win.wrapped.background == background_type::none
			|| win.wrapped.background == background_type::existing_gfx
			|| win.wrapped.texture.empty()
			|| win.wrapped.background == background_type::linechart
			|| win.wrapped.background == background_type::stackedbarchart
			|| win.wrapped.background == background_type::doughnut
			|| win.wrapped.background == background_type::colorsquare
			|| win.wrapped.background == background_type::border_texture_repeat
			|| win.wrapped.background == background_type::textured_corners
			) {
			render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
		} else if(win.wrapped.background == background_type::texture) {
			if(win.wrapped.ogl_texture.loaded == false) {
				win.wrapped.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(win.wrapped.texture));
			}
			if(win.wrapped.ogl_texture.texture_handle == 0) {
				render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
			} else {
				render_textured_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)), win.wrapped.ogl_texture.texture_handle);
			}
		} else if(win.wrapped.background == background_type::bordered_texture) {
			if(win.wrapped.ogl_texture.loaded == false) {
				win.wrapped.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(win.wrapped.texture));
			}
			if(win.wrapped.ogl_texture.texture_handle == 0) {
				render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
			} else {
				render_stretch_textured_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), ui_scale, std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)), win.wrapped.border_size, win.wrapped.ogl_texture.texture_handle);
			}
		}
	}

	if(win.wrapped.share_table_highlight) {
		auto t = table_from_name(open_project, win.wrapped.table_connection);
		if(t) {
			if(!t->table_columns.empty()) {
				int16_t sum = 0;
				for(auto& col : t->table_columns) {
					render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), ((x + sum) * ui_scale), (y * ui_scale), std::max(1, int32_t(col.display_data.width * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
					sum += col.display_data.width;
				}
			}
		}
	}

	// layout
	render_layout(win, win.layout, 1, x, y, win.wrapped.x_size, win.wrapped.y_size, win.wrapped.rectangle_color, ui_scale);
}

void render_control(ui_element_t& c, float x, float y, bool highlighted, float ui_scale) {
	if(c.container_type == container_type::table) {
		if(c.table_columns.empty()) {
			c.x_size = int16_t(open_project.grid_size);
		} else {
			int16_t sum = 0;
			for(auto& col : c.table_columns) {
				sum += col.display_data.width;
			}
			c.x_size = sum;
		}
	}

	auto render_asvg_rect = [&](asvg::svg& s, float hcursor, float vcursor, float x_sz, float y_sz, int32_t gsz) {
		render_hollow_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f),
			hcursor,
			vcursor,
			std::max(1, int32_t(x_sz * ui_scale)),
			std::max(1, int32_t(y_sz * ui_scale)));

		render_textured_rect(color3f{ 0.f, 0.f, 0.f },
			hcursor,
			vcursor,
			std::max(1, int32_t(x_sz * ui_scale)),
			std::max(1, int32_t(y_sz * ui_scale)),
			s.get_render(x_sz / float(gsz), y_sz / float(gsz), gsz, 2.0f));

	};
	auto render_svg_rect = [&](asvg::simple_svg& s, float hcursor, float vcursor, int32_t x_sz, int32_t y_sz, color3f c) {
		render_textured_rect(color3f{ 0.f, 0.f, 0.f },
			hcursor,
			vcursor,
			std::max(1, int32_t(x_sz * ui_scale)),
			std::max(1, int32_t(y_sz * ui_scale)),
			s.get_render(x_sz, y_sz, 2.0f, c.r, c.g, c.b));
	};

	if(c.ttype == template_project::template_type::label) {
		if(c.template_id != -1) {
			auto bg = open_templates.label_t[c.template_id].primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale),  c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		}
		return;
	} 
	if(c.ttype == template_project::template_type::button) {
		if(c.template_id != -1) {
			auto bg = open_templates.button_t[c.template_id].primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		}
		return;
	}
	if(c.ttype == template_project::template_type::free_background) {
		if(c.template_id != -1) 
			render_asvg_rect(open_templates.backgrounds[c.template_id].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
		else
			render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		return;
	}
	if(c.ttype == template_project::template_type::free_icon) {
		if(c.template_id != -1) {

			auto vcursor = y * ui_scale;
			auto hcursor = x * ui_scale;

			render_svg_rect(open_templates.icons[c.template_id].renders,
				hcursor, vcursor, int32_t((c.x_size)), int32_t((c.y_size)),
				c.table_divider_color);
		}
		return;
	}
	if(c.ttype == template_project::template_type::stacked_bar_chart) {
		if(c.template_id != -1) {
			auto bg = open_templates.stacked_bar_t[c.template_id].overlay_bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		}
		return;
	}
	if(c.ttype == template_project::template_type::iconic_button) {
		if(c.template_id != -1) {
			auto bg = open_templates.iconic_button_t[c.template_id].primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));

			auto vcursor = y * ui_scale;
			auto hcursor = x * ui_scale;
			if(c.icon_id != -1) {
				auto l = open_templates.iconic_button_t[c.template_id].primary.icon_left.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto t = open_templates.iconic_button_t[c.template_id].primary.icon_top.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;
				auto r = open_templates.iconic_button_t[c.template_id].primary.icon_right.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto b = open_templates.iconic_button_t[c.template_id].primary.icon_bottom.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;

				hcursor = l;
				vcursor = t;

				render_svg_rect(open_templates.icons[c.icon_id].renders,
					hcursor, vcursor, int32_t((r - l) / ui_scale), int32_t((b - t) / ui_scale),
					open_templates.colors[open_templates.iconic_button_t[c.template_id].primary.icon_color]);
				
			}
		}
		return;
	}
	if(c.ttype == template_project::template_type::mixed_button) {
		if(c.template_id != -1) {
			auto bg = open_templates.mixed_button_t[c.template_id].primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));

			auto vcursor = y * ui_scale;
			auto hcursor = x * ui_scale;
			if(c.icon_id != -1) {
				auto l = open_templates.mixed_button_t[c.template_id].primary.icon_left.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto t = open_templates.mixed_button_t[c.template_id].primary.icon_top.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;
				auto r = open_templates.mixed_button_t[c.template_id].primary.icon_right.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto b = open_templates.mixed_button_t[c.template_id].primary.icon_bottom.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;

				hcursor = l;
				vcursor = t;

				render_svg_rect(open_templates.icons[c.icon_id].renders,
					hcursor, vcursor, int32_t((r - l) / ui_scale), int32_t((b - t) / ui_scale),
					open_templates.colors[open_templates.mixed_button_t[c.template_id].primary.shared_color]);

			}
		}
		return;
	}
	if(c.ttype == template_project::template_type::mixed_button_ci) {
		if(c.template_id != -1) {
			auto bg = open_templates.mixed_button_t[c.template_id].primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));

			auto vcursor = y * ui_scale;
			auto hcursor = x * ui_scale;
			if(c.icon_id != -1) {
				auto l = open_templates.mixed_button_t[c.template_id].primary.icon_left.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto t = open_templates.mixed_button_t[c.template_id].primary.icon_top.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;
				auto r = open_templates.mixed_button_t[c.template_id].primary.icon_right.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + hcursor;
				auto b = open_templates.mixed_button_t[c.template_id].primary.icon_bottom.resolve(float(c.x_size), float(c.y_size), open_project.grid_size) * ui_scale + vcursor;

				hcursor = l;
				vcursor = t;

				render_svg_rect(open_templates.icons[c.icon_id].renders,
					hcursor, vcursor, int32_t((r - l) / ui_scale), int32_t((b - t) / ui_scale),
					c.table_divider_color);

			}
		}
		return;
	}
	if(c.ttype == template_project::template_type::toggle_button) {
		if(c.template_id != -1) {
			auto bg = open_templates.toggle_button_t[c.template_id].on_region.primary.bg;
			if(bg != -1)
				render_asvg_rect(open_templates.backgrounds[bg].renders, (x * ui_scale), (y * ui_scale), c.x_size, c.y_size, open_project.grid_size);
			else
				render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		}
		return;
	}
	if(c.ttype == template_project::template_type::table_header || c.ttype == template_project::template_type::table_row || c.ttype == template_project::template_type::table_highlights) {
		auto t = table_from_name(open_project, c.table_connection);
		if(t) {
			if(!t->table_columns.empty()) {
				int16_t sum = 0;
				for(auto& col : t->table_columns) {
					render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), ((x + sum) * ui_scale), (y * ui_scale), std::max(1, int32_t(col.display_data.width * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
					sum += col.display_data.width;
				}
			}
		}
		return;
	}
	if(c.background == background_type::table_columns || c.background == background_type::table_headers) {
		auto t = table_from_name(open_project, c.table_connection);
		if(t) {
			if(!t->table_columns.empty()) {
				int16_t sum = 0;
				for(auto& col : t->table_columns) {
					render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), ((x + sum) * ui_scale), (y * ui_scale), std::max(1, int32_t(col.display_data.width * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
					sum += col.display_data.width;
				}
			}
		}
	} else if(c.background != background_type::texture && c.background != background_type::bordered_texture && c.background != background_type::progress_bar) {
		if(c.container_type != container_type::table || c.table_columns.empty()) {
			render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		}
	} else if(c.background == background_type::texture || c.background == background_type::progress_bar) {
		if(c.ogl_texture.loaded == false) {
			c.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(c.texture));
		}
		if(c.ogl_texture.texture_handle == 0) {
			render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		} else {
			render_textured_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)), c.ogl_texture.texture_handle);
		}
	} else if(c.background == background_type::bordered_texture) {
		if(c.ogl_texture.loaded == false) {
			c.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(c.texture));
		}
		if(c.ogl_texture.texture_handle == 0) {
			render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
		} else {
			render_stretch_textured_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), (x * ui_scale), (y * ui_scale), ui_scale, std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)), c.border_size, c.ogl_texture.texture_handle);
		}
	}

	if(c.container_type == container_type::table) {
		int16_t sum = 0;
		for(auto& col : c.table_columns) {
			render_empty_rect(c.rectangle_color * (highlighted ? 1.0f : 0.8f), ((x + sum) * ui_scale), (y * ui_scale), std::max(1, int32_t(col.display_data.width * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
			sum += col.display_data.width;
		}
	}
}

struct measure_result {
	int32_t x_space;
	int32_t y_space;
	enum class special {
		none, space_consumer, end_line, end_page, no_break
	} other;
};

struct index_result {
	layout_item* result = nullptr;
	int32_t sub_index = 0;
};

struct layout_iterator {
	std::vector<layout_item>& backing;
	int32_t index = 0;
	int32_t sub_index = 0;

	layout_iterator(std::vector<layout_item>& backing) : backing(backing) { }

	bool current_is_glue() {
		return has_more() && std::holds_alternative<layout_glue_t>(backing[index]);
	}
	measure_result measure_current(window_element_wrapper_t& window, bool glue_horizontal, int32_t max_crosswise) {
		if(!has_more())
			return measure_result{ 0, 0, measure_result::special::none};
		auto& m = backing[index];

		if(std::holds_alternative< layout_control_t>(m)) {
			auto& i = std::get<layout_control_t>(m);
			update_cached_control(i.name, window, i.cached_index);

			if(i.absolute_position) {
				return  measure_result{ 0, 0, measure_result::special::none };
			}
			if(i.cached_index != -1) {
				return  measure_result{ window.children[i.cached_index].x_size, window.children[i.cached_index].y_size, measure_result::special::none };
			}
		} else if(std::holds_alternative<layout_window_t>(m)) {
			auto& i = std::get<layout_window_t>(m);
			update_cached_window(i.name, i.cached_index);
			if(i.absolute_position) {
				return  measure_result{ 0, 0, measure_result::special::none };
			}
			if(i.cached_index != -1) {
				return  measure_result{ open_project.windows[i.cached_index].wrapped.x_size, open_project.windows[i.cached_index].wrapped.y_size, measure_result::special::none };
			}
		} else if(std::holds_alternative<layout_glue_t>(m)) {
			auto& i = std::get<layout_glue_t>(m);
			if(glue_horizontal) {
				switch(i.type) {
					case glue_type::standard: return measure_result{ i.amount, 0, measure_result::special::none };
					case glue_type::at_least: return measure_result{ i.amount, 0, measure_result::special::space_consumer };
					case glue_type::line_break: return measure_result{ 0, 0, measure_result::special::end_line };
					case glue_type::page_break: return measure_result{ 0, 0, measure_result::special::end_page };
					case glue_type::glue_don_t_break: return measure_result{ i.amount, 0, measure_result::special::no_break };
				}
			} else {
				switch(i.type) {
					case glue_type::standard: return measure_result{ 0, i.amount, measure_result::special::none };
					case glue_type::at_least: return measure_result{ 0, i.amount, measure_result::special::space_consumer };
					case glue_type::line_break: return measure_result{ 0, 0, measure_result::special::end_line };
					case glue_type::page_break: return measure_result{ 0, 0, measure_result::special::end_page };
					case glue_type::glue_don_t_break: return measure_result{ 0, i.amount, measure_result::special::no_break };
				}
			}
		} else if(std::holds_alternative<generator_t>(m)) {
			auto& i = std::get<generator_t>(m);
			for(auto& j : i.inserts) {
				update_cached_window(j.name, j.cached_index);
			}
			if(sub_index < int32_t(i.inserts.size()) && i.inserts[sub_index].cached_index != -1) {
				return measure_result{ open_project.windows[i.inserts[sub_index].cached_index].wrapped.x_size, open_project.windows[i.inserts[sub_index].cached_index].wrapped.y_size, measure_result::special::none };
			} else {
				return measure_result{ 0, 0, measure_result::special::none };
			}
		} else if(std::holds_alternative< sub_layout_t>(m)) {
			auto& i = std::get<sub_layout_t>(m);
			int32_t x = 0;
			int32_t y = 0;
			bool consume_fill = false;
			if(i.layout->size_x != -1)
				x = i.layout->size_x;
			else {
				if(glue_horizontal)
					consume_fill = true;
				else
					x = max_crosswise;
			}
			if(i.layout->size_y != -1)
				y = i.layout->size_y;
			else {
				if(!glue_horizontal)
					consume_fill = true;
				else
					y = max_crosswise;
			}

			return measure_result{ x, y, consume_fill ? measure_result::special::space_consumer : measure_result::special::none };
		}
		return measure_result{ 0, 0, measure_result::special::none };
	}
	void render_current(window_element_wrapper_t& window, int layer, float x, float y, int32_t lsz_x, int32_t lsz_y, color3f outline_color, float scale) {
		if(!has_more())
			return;
		auto& m = backing[index];

		if(std::holds_alternative< layout_control_t>(m)) {
			auto& i = std::get<layout_control_t>(m);
			if(i.cached_index != -1) {
				render_control(window.children[i.cached_index], x, y, i.cached_index == selected_control, scale);
				window.children[i.cached_index].x_pos = int16_t(x * scale);
				window.children[i.cached_index].y_pos = int16_t(y * scale);
			}
		} else if(std::holds_alternative<layout_window_t>(m)) {
			auto& i = std::get<layout_window_t>(m);
			if(i.cached_index != -1)
				render_window(open_project.windows[i.cached_index], x, y, false, scale);
		} else if(std::holds_alternative<layout_glue_t>(m)) {

		} else if(std::holds_alternative<generator_t>(m)) {
			auto& i = std::get<generator_t>(m);
			for(auto& j : i.inserts) {
				update_cached_window(j.name, j.cached_index);
			}
			if(sub_index < int32_t(i.inserts.size()) && i.inserts[sub_index].cached_index != -1) {
				render_window(open_project.windows[i.inserts[sub_index].cached_index], x, y, false, scale);
			}
		} else if(std::holds_alternative< sub_layout_t>(m)) {
			auto& i = std::get<sub_layout_t>(m);
			render_layout(window, *(i.layout), layer + 1, x, y, lsz_x, lsz_y, outline_color, scale);
		}
	}
	void move_position(int32_t n) {
		while(n > 0 && has_more()) {
			if(std::holds_alternative<generator_t>(backing[index])) {
				auto& g = std::get<generator_t>(backing[index]);
				++sub_index;
				--n;
				if(sub_index >= int32_t(g.inserts.size())) {
					sub_index = 0;
					++index;
				}
			} else {
				++index;
				--n;
			}
		}
		while(n < 0 && index >= 0) {
			if(std::holds_alternative<generator_t>(backing[index])) {
				auto& g = std::get<generator_t>(backing[index]);
				--sub_index;
				++n;
				if(sub_index < 0) {
					sub_index = 0;
					--index;
				} else {
					continue; // to avoid resetting sub index
				}
			} else {
				--index;
				if(index < 0) {
					index = 0; return;
				}
				++n;
			}

			if(index < 0) {
				index = 0; return;
			}
			if(std::holds_alternative<generator_t>(backing[index])) {
				auto& g = std::get<generator_t>(backing[index]);
				sub_index = std::max(int32_t(g.inserts.size()) - 1, 0);
			}
		}
	}
	bool has_more() {
		return index < int32_t(backing.size());
	}
	void reset() {
		index = 0;
		sub_index = 0;
	}
};

index_result nth_layout_child(layout_level_t& m, int32_t index) {
	int32_t i = 0;
	for(auto& li : m.contents) {
		if(std::holds_alternative<generator_t>(li)) {
			auto& g = std::get<generator_t>(li);
			if(int32_t(i + g.inserts.size()) >= index) {
				return index_result{ &li, index - i };
			}
			i += int32_t(g.inserts.size());
		} else {
			if(i == index)
				return index_result{ &li, 0 };
			++i;
		}
	}
	return index_result{ nullptr, 0 };
}

void render_layout(window_element_wrapper_t& window, layout_level_t& layout, int layer, float x, float y, int32_t width, int32_t height, color3f outline_color, float scale) {
	auto base_x_size = layout.size_x != -1 ? int32_t(layout.size_x) : width;
	auto base_y_size = layout.size_y != -1 ? int32_t(layout.size_y) : height;
	auto top_margin = int32_t(layout.margin_top);
	auto bottom_margin = layout.margin_bottom != -1 ? int32_t(layout.margin_bottom) : top_margin;
	auto left_margin = layout.margin_left != -1 ? int32_t(layout.margin_left) : bottom_margin;
	auto right_margin = layout.margin_right != -1 ? int32_t(layout.margin_right) : left_margin;
	auto effective_x_size = base_x_size - (left_margin + right_margin);
	auto effective_y_size = base_y_size - (top_margin + bottom_margin);
	
	auto id = layout.template_id;
	if(id == -1 && window.wrapped.template_id != -1) {
		id = open_templates.window_t[window.wrapped.template_id].layout_region_definition;
	}
	if(id != -1) {
		auto bg = open_templates.layout_region_t[id].bg;
		if(bg != -1) {
			render_textured_rect(color3f{ 0.f, 0.f, 0.f },
				x * scale,
				y * scale,
				std::max(1, int32_t(base_x_size * scale)),
				std::max(1, int32_t(base_y_size * scale)),
				open_templates.backgrounds[bg].renders.get_render(base_x_size / float(open_project.grid_size), base_y_size / float(open_project.grid_size), open_project.grid_size, 2.0f));
		}
	}

	if(layout.paged) {
		effective_y_size -= int32_t(2 * open_project.grid_size);
	}

	if(layout.open_in_ui) {
		render_layout_rect(outline_color * 2.f * layer, ((x + left_margin) * scale), ((y + top_margin) * scale), std::max(1, int32_t(effective_x_size * scale)), std::max(1, int32_t(effective_y_size * scale)));
	} else {
		render_layout_rect(outline_color * 0.5f * layer, ((x + left_margin) * scale), ((y + top_margin) * scale), std::max(1, int32_t(effective_x_size * scale)), std::max(1, int32_t(effective_y_size * scale)));
	}

	switch(layout.type) {
		case layout_type::single_horizontal:
		{
			float space_used = 0;
			int32_t fill_consumer_count = 0;

			layout_iterator it(layout.contents);

			// measure loop
			layout.page_starts.clear();

			int32_t page_counter = 0;
			while(it.has_more()) {
				auto mr = it.measure_current(window, true, effective_y_size);
				if(layout.paged && (space_used + mr.x_space > effective_x_size || mr.other == measure_result::special::end_page)) {
					if(it.current_is_glue()) {
						++page_counter;
					}
					layout.page_starts.push_back(page_counter);
					//check if previous was glue, and erase
					if(it.index != 0) {
						it.move_position(-1);
						if(it.current_is_glue()) {
							space_used -= it.measure_current(window, true, effective_y_size).x_space;
						}
						it.move_position(1);
					}
					if(it.current_is_glue()) {
						it.move_position(1);
						// normally: break here
					}
					break; // only layout one page
				}

				if(mr.other == measure_result::special::space_consumer)
					++fill_consumer_count;
				space_used += mr.x_space;

				it.move_position(1);
				++page_counter;
			}
			it.reset();

			if(layout.paged) {
				layout.page_starts.push_back( page_counter);
			}
			// place / render

			int32_t extra_runlength = int32_t(effective_x_size - space_used);
			int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
			int32_t extra_lead = 0;
			switch(layout.line_alignment) {
				case layout_line_alignment::leading: break;
				case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
				case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
			}
			space_used = x + extra_lead + left_margin;
			page_counter = 0;
			while(it.has_more() && (!layout.paged || page_counter < layout.page_starts[0])) {
				auto mr = it.measure_current(window, true, effective_y_size);
				float yoff = 0;
				float xoff = space_used;
				switch(layout.line_internal_alignment) {
					case layout_line_alignment::leading: yoff = y + top_margin; break;
					case layout_line_alignment::trailing: yoff = y + top_margin + effective_y_size - mr.y_space; break;
					case layout_line_alignment::centered: yoff = y + top_margin + (effective_y_size - mr.y_space) / 2;  break;
				}
				if(std::holds_alternative< layout_control_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_control_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
					}
				} else if(std::holds_alternative< layout_window_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_window_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
					}
				}
				it.render_current(window, layer, xoff, yoff, mr.x_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), mr.y_space, outline_color, scale);

				space_used += mr.x_space;
				if(mr.other == measure_result::special::space_consumer) {
					space_used += per_fill_consumer;
				}
				it.move_position(1);
				++page_counter;
			}
		} break;
		case layout_type::single_vertical:
		{
			float space_used = 0;
			int32_t fill_consumer_count = 0;

			layout_iterator it(layout.contents);

			// measure loop
			layout.page_starts.clear();

			int32_t page_counter = 0;
			while(it.has_more()) {
				auto mr = it.measure_current(window, false, effective_x_size);
				if(layout.paged && (space_used + mr.y_space > effective_y_size || mr.other == measure_result::special::end_page)) {
					if(it.current_is_glue()) {
						++page_counter;
					}
					layout.page_starts.push_back(page_counter);
					//check if previous was glue, and erase
					if(it.index != 0) {
						it.move_position(-1);
						if(it.current_is_glue()) {
							space_used -= it.measure_current(window, false, effective_x_size).y_space;
						}
						it.move_position(1);
					}
					if(it.current_is_glue()) {
						it.move_position(1);
						// normally: break here
					}
					break; // only layout one page
				}

				if(mr.other == measure_result::special::space_consumer)
					++fill_consumer_count;
				space_used += mr.y_space;

				it.move_position(1);
				++page_counter;
			}
			it.reset();

			if(layout.paged) {
				layout.page_starts.push_back(page_counter);
			}
			// place / render

			int32_t extra_runlength = int32_t(effective_y_size - space_used);
			int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
			int32_t extra_lead = 0;
			switch(layout.line_alignment) {
				case layout_line_alignment::leading: break;
				case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
				case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
			}
			space_used = y + extra_lead + top_margin;
			page_counter = 0;
			while(it.has_more() && (!layout.paged || page_counter < layout.page_starts[0])) {
				auto mr = it.measure_current(window, false, effective_x_size);
				float xoff = 0;
				float yoff = space_used;
				switch(layout.line_internal_alignment) {
					case layout_line_alignment::leading: xoff = x + left_margin; break;
					case layout_line_alignment::trailing: xoff = x + left_margin + effective_x_size - mr.x_space; break;
					case layout_line_alignment::centered: xoff = x + left_margin + (effective_x_size - mr.x_space) / 2;  break;
				}
				if(std::holds_alternative< layout_control_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_control_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
					}
				} else if(std::holds_alternative< layout_window_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_window_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
					}
				}
				it.render_current(window, layer, xoff, yoff, mr.x_space, mr.y_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), outline_color, scale);

				space_used += mr.y_space;
				if(mr.other == measure_result::special::space_consumer) {
					space_used += per_fill_consumer;
				}
				it.move_position(1);
				++page_counter;
			}
		} break;
		case layout_type::overlapped_horizontal:
		{
			float space_used = 0;
			int32_t fill_consumer_count = 0;

			layout_iterator it(layout.contents);

			// measure loop
			layout.page_starts.clear();

			int32_t page_counter = 0;
			int32_t non_glue_count = 0;
			while(it.has_more()) {
				auto mr = it.measure_current(window, true, effective_y_size);
				if(layout.paged && mr.other == measure_result::special::end_page) {
					if(it.current_is_glue()) {
						++page_counter;
					}
					layout.page_starts.push_back(page_counter);
					//check if previous was glue, and erase
					if(it.index != 0) {
						it.move_position(-1);
						if(it.current_is_glue()) {
							space_used -= it.measure_current(window, true, effective_y_size).x_space;
						}
						it.move_position(1);
					}
					if(it.current_is_glue()) {
						it.move_position(1);
						// normally: break here
					}
					break; // only layout one page
				}
				if(!it.current_is_glue()) {
					if((std::holds_alternative<layout_control_t>(it.backing[it.index]) && std::get<layout_control_t>(it.backing[it.index]).absolute_position)
						|| (std::holds_alternative<layout_window_t>(it.backing[it.index]) && std::get<layout_window_t>(it.backing[it.index]).absolute_position)) {

					} else {
						++non_glue_count;
					}
				}

				if(mr.other == measure_result::special::space_consumer)
					++fill_consumer_count;
				space_used += mr.x_space;

				it.move_position(1);
				++page_counter;
			}
			it.reset();

			if(layout.paged) {
				layout.page_starts.push_back(page_counter);
			}
			// place / render

			int32_t extra_runlength = std::max(0, int32_t(effective_x_size - space_used));
			int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
			int32_t extra_lead = 0;
			switch(layout.line_alignment) {
				case layout_line_alignment::leading: break;
				case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
				case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
			}
			int32_t overlap_subtraction = (non_glue_count > 1 && space_used > effective_x_size) ? int32_t(space_used - effective_x_size) / (non_glue_count - 1) : 0;
			space_used = x + extra_lead + left_margin;
			page_counter = 0;
			while(it.has_more() && (!layout.paged || page_counter < layout.page_starts[0])) {
				auto mr = it.measure_current(window, true, effective_y_size);
				float yoff = 0;
				float xoff = space_used;
				switch(layout.line_internal_alignment) {
					case layout_line_alignment::leading: yoff = y + top_margin; break;
					case layout_line_alignment::trailing: yoff = y + top_margin + effective_y_size - mr.y_space; break;
					case layout_line_alignment::centered: yoff = y + top_margin + (effective_y_size - mr.y_space) / 2;  break;
				}
				bool was_abs = false;
				if(std::holds_alternative< layout_control_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_control_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
						was_abs = true;
					}
				} else if(std::holds_alternative< layout_window_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_window_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
						was_abs = true;
					}
				}
				it.render_current(window, layer, xoff, yoff, mr.x_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), mr.y_space, outline_color, scale);

				space_used += mr.x_space;
				if(mr.other == measure_result::special::space_consumer) {
					space_used += per_fill_consumer;
				}
				if(!it.current_is_glue() && !was_abs)
					space_used -= overlap_subtraction;

				it.move_position(1);
				++page_counter;
			}
		} break;
		case layout_type::overlapped_vertical:
		{
			float space_used = 0;
			int32_t fill_consumer_count = 0;

			layout_iterator it(layout.contents);

			// measure loop
			layout.page_starts.clear();

			int32_t page_counter = 0;
			int32_t non_glue_count = 0;
			while(it.has_more()) {
				auto mr = it.measure_current(window, false, effective_x_size);
				if(layout.paged && mr.other == measure_result::special::end_page) {
					if(it.current_is_glue()) {
						++page_counter;
					}
					layout.page_starts.push_back(page_counter);
					//check if previous was glue, and erase
					if(it.index != 0) {
						it.move_position(-1);
						if(it.current_is_glue()) {
							space_used -= it.measure_current(window, false, effective_x_size).y_space;
						}
						it.move_position(1);
					}
					if(it.current_is_glue()) {
						it.move_position(1);
						// normally: break here
					}
					break; // only layout one page
				}
				if(!it.current_is_glue()) {
					if((std::holds_alternative<layout_control_t>(it.backing[it.index]) && std::get<layout_control_t>(it.backing[it.index]).absolute_position)
						|| (std::holds_alternative<layout_window_t>(it.backing[it.index]) && std::get<layout_window_t>(it.backing[it.index]).absolute_position)) {

					} else {
						++non_glue_count;
					}
				}

				if(mr.other == measure_result::special::space_consumer)
					++fill_consumer_count;
				space_used += mr.y_space;

				it.move_position(1);
				++page_counter;
			}
			it.reset();

			if(layout.paged) {
				layout.page_starts.push_back(page_counter);
			}
			// place / render

			int32_t extra_runlength = std::max(0, int32_t(effective_y_size - space_used));
			int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
			int32_t extra_lead = 0;
			switch(layout.line_alignment) {
				case layout_line_alignment::leading: break;
				case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
				case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
			}
			int32_t overlap_subtraction = (non_glue_count > 1 && space_used > effective_y_size) ? int32_t(space_used - effective_y_size) / (non_glue_count - 1) : 0;
			space_used = y + extra_lead + top_margin;
			page_counter = 0;
			while(it.has_more() && (!layout.paged || page_counter < layout.page_starts[0])) {
				auto mr = it.measure_current(window, false, effective_x_size);
				float xoff = 0;
				float yoff = space_used;
				switch(layout.line_internal_alignment) {
					case layout_line_alignment::leading: xoff = x + left_margin; break;
					case layout_line_alignment::trailing: xoff = x + left_margin + effective_x_size - mr.x_space; break;
					case layout_line_alignment::centered: xoff = x + left_margin + (effective_x_size - mr.x_space) / 2;  break;
				}
				bool was_abs = false;
				if(std::holds_alternative< layout_control_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_control_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
						was_abs = true;
					}
				} else if(std::holds_alternative< layout_control_t>(layout.contents[it.index])) {
					auto& i = std::get<layout_control_t>(layout.contents[it.index]);
					if(i.absolute_position) {
						xoff = x + i.abs_x;
						yoff = y + i.abs_y;
						was_abs = true;
					}
				}
				it.render_current(window, layer, xoff, yoff, mr.x_space, mr.y_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), outline_color, scale);

				space_used += mr.y_space;
				if(mr.other == measure_result::special::space_consumer) {
					space_used += per_fill_consumer;
				}
				if(!it.current_is_glue() && !was_abs)
					space_used -= overlap_subtraction;

				it.move_position(1);
				++page_counter;
			}
		} break;
		case layout_type::mulitline_horizontal:
		{
			layout_iterator it(layout.contents);

			layout.page_starts.clear();

			int32_t page_counter = 0;
			int32_t crosswise_used = 0;

			// loop for page
			while(it.has_more()) {
				auto line_start = it;
				int32_t max_crosswise = 0;
				float space_used = 0;
				int32_t fill_consumer_count = 0;

				// loop for line
				bool page_ended = false;
				while(it.has_more()) {
					auto mr = it.measure_current(window, true, effective_y_size - crosswise_used);
					if((space_used > 0 && (space_used + mr.x_space > effective_x_size || mr.other == measure_result::special::end_line)) || (space_used > 0 && mr.other == measure_result::special::end_page) || (crosswise_used > 0 && mr.other == measure_result::special::end_page) || (layout.paged && crosswise_used + mr.y_space > effective_y_size && crosswise_used > 0)) {
						if(it.current_is_glue()) {
							++page_counter;
						}
						if(mr.other == measure_result::special::end_page || (crosswise_used + mr.y_space > effective_y_size && crosswise_used > 0)) {
							page_ended = true;
						}

						//check if previous was glue, and erase
						if(it.index != 0) {
							it.move_position(-1);
							if(it.current_is_glue()) {
								space_used -= it.measure_current(window, true, effective_y_size - crosswise_used).x_space;
							}
							it.move_position(1);
						}
						if(it.current_is_glue()) {
							it.move_position(1);
							// normally: break here
						}
						break; // only one line
					}

					max_crosswise = std::max(max_crosswise, mr.y_space);
					if(mr.other == measure_result::special::space_consumer)
						++fill_consumer_count;
					space_used += mr.x_space;

					it.move_position(1);
					++page_counter;
				}

				// position/render line
				int32_t extra_runlength = int32_t(effective_x_size - space_used);
				int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
				int32_t extra_lead = 0;
				switch(layout.line_alignment) {
					case layout_line_alignment::leading: break;
					case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
					case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
				}
				space_used = x + extra_lead + left_margin;
				page_counter = 0;

				while(line_start.index < it.index || line_start.sub_index < it.sub_index) {
					auto mr = line_start.measure_current(window, true, effective_y_size);
					float yoff = 0;
					float xoff = space_used;
					switch(layout.line_internal_alignment) {
						case layout_line_alignment::leading: yoff = y + crosswise_used + top_margin; break;
						case layout_line_alignment::trailing: yoff = y + crosswise_used + top_margin + max_crosswise - mr.y_space; break;
						case layout_line_alignment::centered: yoff = y + crosswise_used + top_margin + (max_crosswise - mr.y_space) / 2;  break;
					}
					if(std::holds_alternative< layout_control_t>(layout.contents[line_start.index])) {
						auto& i = std::get<layout_control_t>(layout.contents[line_start.index]);
						if(i.absolute_position) {
							xoff = x + i.abs_x;
							yoff = y + i.abs_y;
						}
					} else if(std::holds_alternative< layout_window_t>(layout.contents[line_start.index])) {
						auto& i = std::get<layout_window_t>(layout.contents[line_start.index]);
						if(i.absolute_position) {
							xoff = x + i.abs_x;
							yoff = y + i.abs_y;
						}
					}
					line_start.render_current(window, layer, xoff, yoff, mr.x_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), mr.y_space, outline_color, scale);

					space_used += mr.x_space;
					if(mr.other == measure_result::special::space_consumer) {
						space_used += per_fill_consumer;
					}
					line_start.move_position(1);
				}

				crosswise_used += max_crosswise;
				crosswise_used += int32_t(layout.interline_spacing);

				if(layout.paged && (crosswise_used >= effective_y_size || page_ended)) {
					layout.page_starts.push_back(page_counter);
					// normally, make new page here ...
					break;
				}
			}
			// last page, if not added
			if(layout.paged) {
				if(layout.page_starts.empty() || layout.page_starts.back() != page_counter)
					layout.page_starts.push_back(page_counter);
			}
		} break;
		case layout_type::multiline_vertical:
		{
			layout_iterator it(layout.contents);

			layout.page_starts.clear();

			int32_t page_counter = 0;
			int32_t crosswise_used = 0;

			// loop for page
			while(it.has_more()) {
				auto line_start = it;
				int32_t max_crosswise = 0;
				float space_used = 0;
				int32_t fill_consumer_count = 0;

				// loop for line
				bool page_ended = false;
				while(it.has_more()) {
					auto mr = it.measure_current(window, false, effective_x_size - crosswise_used);
					if((space_used > 0 && (space_used + mr.y_space > effective_y_size || mr.other == measure_result::special::end_line)) || (space_used > 0 && mr.other == measure_result::special::end_page) || (crosswise_used > 0 && mr.other == measure_result::special::end_page) || (layout.paged && crosswise_used + mr.x_space > effective_x_size && crosswise_used > 0)) {
						if(it.current_is_glue()) {
							++page_counter;
						}
						if(mr.other == measure_result::special::end_page || (crosswise_used + mr.x_space > effective_x_size && crosswise_used > 0)) {
							page_ended = true;
						}

						//check if previous was glue, and erase
						if(it.index != 0) {
							it.move_position(-1);
							if(it.current_is_glue()) {
								space_used -= it.measure_current(window, false, effective_x_size - crosswise_used).y_space;
							}
							it.move_position(1);
						}
						if(it.current_is_glue()) {
							it.move_position(1);
							// normally: break here
						}
						break; // only one line
					}

					max_crosswise = std::max(max_crosswise, mr.x_space);
					if(mr.other == measure_result::special::space_consumer)
						++fill_consumer_count;
					space_used += mr.y_space;

					it.move_position(1);
					++page_counter;
				}

				// position/render line
				int32_t extra_runlength = int32_t(effective_y_size - space_used);
				int32_t per_fill_consumer = fill_consumer_count != 0 ? (extra_runlength / fill_consumer_count) : 0;
				int32_t extra_lead = 0;
				switch(layout.line_alignment) {
					case layout_line_alignment::leading: break;
					case layout_line_alignment::trailing: extra_lead = extra_runlength - fill_consumer_count * per_fill_consumer; break;
					case layout_line_alignment::centered: extra_lead = (extra_runlength - fill_consumer_count * per_fill_consumer) / 2;  break;
				}
				space_used = y + extra_lead + top_margin;
				page_counter = 0;

				while(line_start.index < it.index || line_start.sub_index < it.sub_index) {
					auto mr = line_start.measure_current(window, false, effective_x_size);
					float xoff = 0;
					float yoff = space_used;
					switch(layout.line_internal_alignment) {
						case layout_line_alignment::leading: xoff = x + crosswise_used + left_margin; break;
						case layout_line_alignment::trailing: xoff = x + crosswise_used + left_margin + max_crosswise - mr.x_space; break;
						case layout_line_alignment::centered: xoff = x + crosswise_used + left_margin + (max_crosswise - mr.x_space) / 2;  break;
					}
					if(std::holds_alternative< layout_control_t>(layout.contents[line_start.index])) {
						auto& i = std::get<layout_control_t>(layout.contents[line_start.index]);
						if(i.absolute_position) {
							xoff = x + i.abs_x;
							yoff = y + i.abs_y;
						}
					} else if(std::holds_alternative< layout_window_t>(layout.contents[line_start.index])) {
						auto& i = std::get<layout_window_t>(layout.contents[line_start.index]);
						if(i.absolute_position) {
							xoff = x + i.abs_x;
							yoff = y + i.abs_y;
						}
					}
					line_start.render_current(window, layer, xoff, yoff, mr.x_space , mr.y_space + (mr.other == measure_result::special::space_consumer ? per_fill_consumer : 0), outline_color, scale);

					space_used += mr.y_space;
					if(mr.other == measure_result::special::space_consumer) {
						space_used += per_fill_consumer;
					}
					line_start.move_position(1);
				}

				crosswise_used += max_crosswise;
				crosswise_used += int32_t(layout.interline_spacing);

				if(layout.paged && (crosswise_used >= effective_x_size || page_ended)) {
					layout.page_starts.push_back(page_counter);
					// normally, make new page here ...
					break;
				}
			}
			// last page, if not added
			if(layout.paged) {
				if(layout.page_starts.empty() || layout.page_starts.back() != page_counter)
					layout.page_starts.push_back(page_counter);
			}
		} break;
	}
}

void rename_window(layout_level_t& layout, std::string const& old_name, std::string const& new_name) {
	for(auto& m : layout.contents) {
		if(std::holds_alternative< layout_control_t>(m)) {
			auto& i = std::get<layout_control_t>(m);

		} else if(std::holds_alternative<layout_window_t>(m)) {
			auto& i = std::get<layout_window_t>(m);
			if(i.name == old_name)
				i.name = new_name;
		} else if(std::holds_alternative<layout_glue_t>(m)) {

		} else if(std::holds_alternative<generator_t>(m)) {
			auto& i = std::get<generator_t>(m);
			for(auto& p : i.inserts) {
				if(p.name == old_name)
					p.name = new_name;
				if(p.header == old_name)
					p.header = new_name;
			}
		} else if(std::holds_alternative< sub_layout_t>(m)) {
			auto& i = std::get<sub_layout_t>(m);
			rename_window(*i.layout, old_name, new_name);
		}
	}
}
void rename_control(layout_level_t& layout, std::string const& old_name, std::string const& new_name) {
	for(auto& m : layout.contents) {
		if(std::holds_alternative< layout_control_t>(m)) {
			auto& i = std::get<layout_control_t>(m);
			if(i.name == old_name)
				i.name = new_name;
		} else if(std::holds_alternative<layout_window_t>(m)) {

		} else if(std::holds_alternative<layout_glue_t>(m)) {

		} else if(std::holds_alternative<generator_t>(m)) {

		} else if(std::holds_alternative< sub_layout_t>(m)) {
			auto& i = std::get<sub_layout_t>(m);
			rename_control(*i.layout, old_name, new_name);
		}
	}
}

void apply_layout_style(layout_level_t& layout, int16_t margins, int16_t header_height, bool first) {
	int internal_layouts = 0;
	bool is_first = true;
	for(auto& m : layout.contents) {
		if(std::holds_alternative< sub_layout_t>(m)) {
			internal_layouts++;
			auto& i = std::get<sub_layout_t>(m);
			apply_layout_style(*i.layout, margins, header_height, is_first);
			is_first = false;
		}
	}

	if (internal_layouts == 0) {
		layout.margin_top = margins;
	}
	if (internal_layouts == 0 && first && layout.size_y != -1) {
		layout.size_y = header_height;
	}
}

void template_type_options(template_project::template_type& ttype, int16_t& template_id) {
	std::vector<char const*> opts;
	opts.push_back("None");
	opts.push_back("Label");
	opts.push_back("Text button");
	opts.push_back("Icon button");
	opts.push_back("Text & icon button");
	opts.push_back("Toggle button");
	opts.push_back("Table header");
	opts.push_back("Table row");
	opts.push_back("Text & icon button (color icon)");
	opts.push_back("Stacked bar chart");
	opts.push_back("Table highlights");
	opts.push_back("Icon");
	opts.push_back("Background image");

	int32_t current = 0;
	switch(ttype) {
		case template_project::template_type::none:
			current = 0; break;
		case template_project::template_type::label:
			current = 1; break;
		case template_project::template_type::button:
			current = 2; break;
		case template_project::template_type::iconic_button:
			current = 3; break;
		case template_project::template_type::mixed_button:
			current = 4; break;
		case template_project::template_type::toggle_button:
			current = 5; break;
		case template_project::template_type::table_header:
			current = 6; break;
		case template_project::template_type::table_row:
			current = 7; break;
		case template_project::template_type::mixed_button_ci:
			current = 8; break;
		case template_project::template_type::stacked_bar_chart:
			current = 9; break;
		case template_project::template_type::table_highlights:
			current = 10; break;
		case template_project::template_type::free_icon:
			current = 11; break;
		case template_project::template_type::free_background:
			current = 12; break;
	}

	if(ImGui::Combo("Template type", &current, opts.data(), int32_t(opts.size()))) {
		switch(current) {
			case 0:
				ttype = template_project::template_type::none; break;
			case 1:
				ttype = template_project::template_type::label; break;
			case 2:
				ttype = template_project::template_type::button; break;
			case 3:
				ttype = template_project::template_type::iconic_button; break;
			case 4:
				ttype = template_project::template_type::mixed_button; break;
			case 5:
				ttype = template_project::template_type::toggle_button; break;
			case 6:
				ttype = template_project::template_type::table_header; break;
			case 7:
				ttype = template_project::template_type::table_row; break;
			case 8:
				ttype = template_project::template_type::mixed_button_ci; break;
			case 9:
				ttype = template_project::template_type::stacked_bar_chart; break;
			case 10:
				ttype = template_project::template_type::table_highlights; break;
			case 11:
				ttype = template_project::template_type::free_icon; break;
			case 12:
				ttype = template_project::template_type::free_background; break;
			default:
				break;
		}
	}

	switch(ttype) {
		case template_project::template_type::none:
			break;
		case template_project::template_type::label:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.label_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::button:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.button_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::iconic_button:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.iconic_button_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::mixed_button:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.mixed_button_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::mixed_button_ci:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.mixed_button_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::toggle_button:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.toggle_button_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::progress_bar:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.progress_bar_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::stacked_bar_chart:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.stacked_bar_t) {
				inner_opts.push_back(i.display_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::free_icon:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.icons) {
				inner_opts.push_back(i.file_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
		case template_project::template_type::free_background:
		{
			std::vector<char const*> inner_opts;
			inner_opts.push_back("None");
			for(auto& i : open_templates.backgrounds) {
				inner_opts.push_back(i.file_name.c_str());
			}
			int32_t chosen = template_id + 1;
			if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
				template_id = int16_t(chosen - 1);
			}
		} break;
	}
}

void control_options(window_element_wrapper_t& win, ui_element_t& c) {

	if(ImGui::Button("Delete")) {
		win.children.erase(win.children.begin() + selected_control);
		selected_control = -1;
		return;
	}

	ImGui::SameLine();
	if(ImGui::Button("Copy")) {
		win.children.push_back(win.children[selected_control]);
		win.children.back().name += "_copy";
	}

	ImGui::InputText("Name", &(c.temp_name));
	ImGui::SameLine();
	if(ImGui::Button("Change Name")) {
		if(c.temp_name.empty()) {
			MessageBoxW(nullptr, L"Name cannot be empty", L"Invalid Name", MB_OK);
		} else {
			bool found = false;
			for(auto& w : win.children) {
				if(w.name == c.temp_name) {
					found = true;
					break;
				}
			}
			if(found) {
				MessageBoxW(nullptr, L"Name must be unique", L"Invalid Name", MB_OK);
			} else {
				for(auto& alt : win.alternates) {
					if(alt.control_name == c.name)
						alt.control_name = c.temp_name;
				}
				rename_control(win.layout, c.name, c.temp_name);
				c.name = c.temp_name;
			}
		}
	}



	int32_t temp = 0;

	temp = c.x_size;
	ImGui::InputInt("Width", &temp);
	c.x_size = int16_t(std::max(0, temp));

	temp = c.y_size;
	ImGui::InputInt("Height", &temp);
	c.y_size = int16_t(std::max(0, temp));

	ImVec4 ccolor{ c.rectangle_color.r, c.rectangle_color.b, c.rectangle_color.g, 1.0f };
	ImGui::ColorEdit3("Outline color", (float*)&ccolor);
	c.rectangle_color.r = ccolor.x;
	c.rectangle_color.g = ccolor.z;
	c.rectangle_color.b = ccolor.y;

	ImGui::Checkbox("Ignore grid", &(c.no_grid));

	template_type_options(c.ttype, c.template_id);

	switch(c.ttype) {
		case template_project::template_type::label:
		{
			ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
			if(!c.dynamic_text) 
				ImGui::InputText("Text key", &(c.text_key));
			
			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip) 
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));
			
			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::button:
		{
			ImGui::Checkbox("Left-click action", &(c.left_click_action));
			ImGui::Checkbox("Right-click action", &(c.right_click_action));
			ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
			if(c.left_click_action || c.right_click_action || c.shift_click_action) {
				ImGui::InputText("Hotkey", &(c.hotkey));
			}
			ImGui::Checkbox("Hover activation", &(c.hover_activation));

			ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
			if(!c.dynamic_text)
				ImGui::InputText("Text key", &(c.text_key));

			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::toggle_button:
		{
			ImGui::Checkbox("Left-click action", &(c.left_click_action));
			ImGui::Checkbox("Right-click action", &(c.right_click_action));
			ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
			if(c.left_click_action || c.right_click_action || c.shift_click_action) {
				ImGui::InputText("Hotkey", &(c.hotkey));
			}
			ImGui::Checkbox("Hover activation", &(c.hover_activation));

			ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
			if(!c.dynamic_text)
				ImGui::InputText("Text key", &(c.text_key));

			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::iconic_button:
		{
			ImGui::Checkbox("Left-click action", &(c.left_click_action));
			ImGui::Checkbox("Right-click action", &(c.right_click_action));
			ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
			if(c.left_click_action || c.right_click_action || c.shift_click_action) {
				ImGui::InputText("Hotkey", &(c.hotkey));
			}
			ImGui::Checkbox("Hover activation", &(c.hover_activation));

			{
				std::vector<char const*> inner_opts;
				inner_opts.push_back("None");
				for(auto& i : open_templates.icons) {
					inner_opts.push_back(i.file_name.c_str());
				}
				int32_t chosen = c.icon_id + 1;
				if(ImGui::Combo("Default icon", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
					c.icon_id = int16_t(chosen - 1);
				}
			}

			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::mixed_button:
		{
			ImGui::Checkbox("Left-click action", &(c.left_click_action));
			ImGui::Checkbox("Right-click action", &(c.right_click_action));
			ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
			if(c.left_click_action || c.right_click_action || c.shift_click_action) {
				ImGui::InputText("Hotkey", &(c.hotkey));
			}
			ImGui::Checkbox("Hover activation", &(c.hover_activation));

			ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
			if(!c.dynamic_text)
				ImGui::InputText("Text key", &(c.text_key));

			{
				std::vector<char const*> inner_opts;
				inner_opts.push_back("None");
				for(auto& i : open_templates.icons) {
					inner_opts.push_back(i.file_name.c_str());
				}
				int32_t chosen = c.icon_id + 1;
				if(ImGui::Combo("Default icon", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
					c.icon_id = int16_t(chosen - 1);
				}
			}

			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::mixed_button_ci:
		{
			ImGui::Checkbox("Left-click action", &(c.left_click_action));
			ImGui::Checkbox("Right-click action", &(c.right_click_action));
			ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
			if(c.left_click_action || c.right_click_action || c.shift_click_action) {
				ImGui::InputText("Hotkey", &(c.hotkey));
			}
			ImGui::Checkbox("Hover activation", &(c.hover_activation));

			ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
			if(!c.dynamic_text)
				ImGui::InputText("Text key", &(c.text_key));
			{
				{
					float ccolor[3] = { c.table_divider_color.r, c.table_divider_color.g, c.table_divider_color.b };
					ImGui::ColorEdit3("Default icon color", ccolor);
					c.table_divider_color.r = ccolor[0];
					c.table_divider_color.g = ccolor[1];
					c.table_divider_color.b = ccolor[2];
				}
			}
			{
				std::vector<char const*> inner_opts;
				inner_opts.push_back("None");
				for(auto& i : open_templates.icons) {
					inner_opts.push_back(i.file_name.c_str());
				}
				int32_t chosen = c.icon_id + 1;
				if(ImGui::Combo("Default icon", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
					c.icon_id = int16_t(chosen - 1);
				}
			}

			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::free_icon:
		{
			{
				{
					float ccolor[3] = { c.table_divider_color.r, c.table_divider_color.g, c.table_divider_color.b };
					ImGui::ColorEdit3("Default icon color", ccolor);
					c.table_divider_color.r = ccolor[0];
					c.table_divider_color.g = ccolor[1];
					c.table_divider_color.b = ccolor[2];
				}
			}
			
			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));
			ImGui::Checkbox("Other dynamic behavior", &(c.dynamic_text));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::free_background:
		{
			ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
			if(!c.dynamic_tooltip)
				ImGui::InputText("Tooltip key", &(c.tooltip_text_key));
			ImGui::Checkbox("Other dynamic behavior", &(c.dynamic_text));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		case template_project::template_type::table_row:
		{
			std::vector<char const*> table_names;
			table_names.push_back("--None--");
			int32_t selection = (c.table_connection == "" ? 0 : -1);
			for(auto& tc : open_project.tables) {
				if(tc.template_id != -1) {
					table_names.push_back(tc.name.c_str());
					if(tc.name == c.table_connection) {
						selection = int32_t(table_names.size() - 1);
					}
				}
			}

			temp = selection;
			ImGui::Combo("Associated table", &selection, table_names.data(), int32_t(table_names.size()));
			if(temp != selection) {
				if(selection == 0) {
					c.table_connection = "";
					c.template_id = -1;
				} else {
					c.table_connection = std::string(table_names[selection]);
					c.template_id = table_from_name(open_project, c.table_connection)->template_id;
				}
			}
		} break;
		case template_project::template_type::table_highlights:
		{
			std::vector<char const*> table_names;
			table_names.push_back("--None--");
			int32_t selection = (c.table_connection == "" ? 0 : -1);
			for(auto& tc : open_project.tables) {
				if(tc.template_id != -1) {
					table_names.push_back(tc.name.c_str());
					if(tc.name == c.table_connection) {
						selection = int32_t(table_names.size() - 1);
					}
				}
			}

			temp = selection;
			ImGui::Combo("Associated table", &selection, table_names.data(), int32_t(table_names.size()));
			if(temp != selection) {
				if(selection == 0) {
					c.table_connection = "";
					c.template_id = -1;
				} else {
					c.table_connection = std::string(table_names[selection]);
					c.template_id = table_from_name(open_project, c.table_connection)->template_id;
				}
			}
		} break;
		case template_project::template_type::table_header:
		{
			std::vector<char const*> table_names;
			table_names.push_back("--None--");
			int32_t selection = (c.table_connection == "" ? 0 : -1);
			for(auto& tc : open_project.tables) {
				if(tc.template_id != -1) {
					table_names.push_back(tc.name.c_str());
					if(tc.name == c.table_connection) {
						selection = int32_t(table_names.size() - 1);
					}
				}
			}

			temp = selection;
			ImGui::Combo("Associated table", &selection, table_names.data(), int32_t(table_names.size()));
			if(temp != selection) {
				if(selection == 0) {
					c.table_connection = "";
					c.template_id = -1;
				} else {
					c.table_connection = std::string(table_names[selection]);
					c.template_id = table_from_name(open_project, c.table_connection)->template_id;
				}
			}
		} break;
		case template_project::template_type::stacked_bar_chart:
		{
			int32_t dptemp = c.datapoints;
			ImGui::InputInt("Data points", &dptemp);
			c.datapoints = int16_t(dptemp);

			ImGui::InputText("Data key", &c.list_content);
			ImGui::Checkbox("Don't sort", &(c.has_alternate_bg));

			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
		} break;
		default:
		{
			ImGui::Text("Lua elements allow to add dynamic elements without generation and recompilation of c++ code. Current supports only a subset of features.");
			ImGui::Checkbox("Lua", &(c.is_lua));

			if(is_lua_element(c)) {
				c.dynamic_tooltip = false;
				ImGui::InputText("Tooltip key##lua_tooltip_key", &(c.tooltip_text_key));
				ImGui::InputText("On update function", &(c.text_key));
				{
					const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
					temp = int32_t(c.text_color);
					ImGui::Combo("Text color#lua_text_color", &temp, items, 16);
					c.text_color = text_color(temp);
				}
				{
					const char* items[] = { "body", "header" };
					temp = int32_t(c.text_type);
					ImGui::Combo("Text style#lua_text_style", &temp, items, 2);
					c.text_type = text_type(temp);
				}
				{
					const char* items[] = { "left (leading)", "right (trailing)", "center" };
					temp = int32_t(c.text_align);
					ImGui::Combo("Text alignment#lua_text_align", &temp, items, 3);
					c.text_align = aui_text_alignment(temp);
				}
			}

			if(is_lua_element(c)) {
				ImGui::BeginDisabled();
			}

			{
				const char* items[] = { "none", "texture", "bordered texture", "legacy GFX", "line chart", "stacked bar chart", "solid color", "flag", "table columns", "table headers", "progress bar", "icon strip", "doughnut", "border repeat", "corners" };
				temp = int32_t(c.background);
				ImGui::Combo("Background", &temp, items, 15);
				c.background = background_type(temp);
			}

			if(c.background == background_type::table_columns || c.background == background_type::table_headers) {
				std::vector<char const*> table_names;
				table_names.push_back("[none]");
				int32_t selection = (c.table_connection == "" ? 0 : -1);
				for(auto& tc : open_project.tables) {
					table_names.push_back(tc.name.c_str());
					if(tc.name == c.table_connection) {
						selection = int32_t(table_names.size() - 1);
					}
				}

				temp = selection;
				ImGui::Combo("Associated table", &selection, table_names.data(), int32_t(table_names.size()));
				if(temp != selection) {
					if(selection == 0)
						c.table_connection = "";
					else
						c.table_connection = open_project.tables[selection - 1].name;
				}
			} else if(c.background == background_type::existing_gfx) {
				ImGui::InputText("Texture", &(c.texture));
			} else if(background_type_is_textured(c.background)) {
				std::string tex = "Texture: " + (c.texture.size() > 0 ? c.texture : std::string("[none]"));
				ImGui::Text(tex.c_str());
				ImGui::SameLine();
				if(ImGui::Button("Change")) {
					auto new_file = fs::pick_existing_file(L"");
					//auto breakpt = new_file.find_last_of(L'\\');
					//c.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
					c.texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					c.ogl_texture.unload();
				}
				ImGui::Checkbox("Has alternate background", &(c.has_alternate_bg));
				if(c.has_alternate_bg) {
					std::string tex = "Alternate texture: " + (c.alternate_bg.size() > 0 ? c.alternate_bg : std::string("[none]"));
					ImGui::Text(tex.c_str());
					ImGui::SameLine();
					if(ImGui::Button("Change Alternate")) {
						auto new_file = fs::pick_existing_file(L"");
						//auto breakpt = new_file.find_last_of(L'\\');
						//c.alternate_bg = fs::native_to_utf8(new_file.substr(breakpt + 1));
						c.alternate_bg = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					}
				}
			} else if(c.background == background_type::progress_bar) {
				std::string tex = "Texture: " + (c.texture.size() > 0 ? c.texture : std::string("[none]"));
				ImGui::Text(tex.c_str());
				ImGui::SameLine();
				if(ImGui::Button("Change")) {
					auto new_file = fs::pick_existing_file(L"");
					//auto breakpt = new_file.find_last_of(L'\\');
					//c.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
					c.texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					c.ogl_texture.unload();
				}

				tex = "Alternate texture: " + (c.alternate_bg.size() > 0 ? c.alternate_bg : std::string("[none]"));
				ImGui::Text(tex.c_str());
				ImGui::SameLine();
				if(ImGui::Button("Change Alternate")) {
					auto new_file = fs::pick_existing_file(L"");
					//auto breakpt = new_file.find_last_of(L'\\');
					//c.alternate_bg = fs::native_to_utf8(new_file.substr(breakpt + 1));
					c.alternate_bg = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
				}
			} else if(c.background == background_type::linechart) {
				temp = c.datapoints;
				ImGui::InputInt("Data points", &temp);
				c.datapoints = int16_t(temp);
				{
					ImVec4 ccolor{ c.other_color.r, c.other_color.g, c.other_color.b, c.other_color.a };
					ImGui::ColorEdit4("line color", (float*)&ccolor);
					c.other_color.r = ccolor.x;
					c.other_color.g = ccolor.y;
					c.other_color.b = ccolor.z;
					c.other_color.a = ccolor.w;
				}
			} else if(c.background == background_type::stackedbarchart) {
				temp = c.datapoints;
				ImGui::InputInt("Data points", &temp);
				c.datapoints = int16_t(temp);

				ImGui::InputText("Data key", &c.list_content);
				ImGui::Checkbox("Don't sort", &(c.has_alternate_bg));
			} else if(c.background == background_type::doughnut) {
				temp = c.datapoints;
				ImGui::InputInt("Data points", &temp);
				c.datapoints = int16_t(temp);

				ImGui::InputText("Data key", &c.list_content);
				ImGui::Checkbox("Don't sort", &(c.has_alternate_bg));
			} else if(c.background == background_type::colorsquare) {
				{
					ImVec4 ccolor{ c.other_color.r, c.other_color.g, c.other_color.b, c.other_color.a };
					ImGui::ColorEdit4("Default color", (float*)&ccolor);
					c.other_color.r = ccolor.x;
					c.other_color.g = ccolor.y;
					c.other_color.b = ccolor.z;
					c.other_color.a = ccolor.w;
				}
			}
			if(c.background == background_type::bordered_texture) {
				temp = c.border_size;
				ImGui::InputInt("Border size", &temp);
				c.border_size = int16_t(std::max(0, temp));
			}
			ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
			{
				const char* items[] = { "none", "list", "grid", "table" };
				temp = int32_t(c.container_type);
				ImGui::Combo("Act as container", &temp, items, 4);
				c.container_type = container_type(temp);
			}

			if(c.container_type == container_type::none) {
				ImGui::Checkbox("Dynamic text", &(c.dynamic_text));
				if(!c.dynamic_text) {
					ImGui::InputText("Text key", &(c.text_key));
				}
				ImGui::InputFloat("Text scale", &(c.text_scale));
				c.text_scale = std::max(0.01f, c.text_scale);

				{
					const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
					temp = int32_t(c.text_color);
					ImGui::Combo("Text color", &temp, items, 16);
					c.text_color = text_color(temp);
				}
				{
					const char* items[] = { "body", "header" };
					temp = int32_t(c.text_type);
					ImGui::Combo("Text style", &temp, items, 2);
					c.text_type = text_type(temp);
				}
				{
					const char* items[] = { "left (leading)", "right (trailing)", "center" };
					temp = int32_t(c.text_align);
					ImGui::Combo("Text alignment", &temp, items, 3);
					c.text_align = aui_text_alignment(temp);
				}

				ImGui::Checkbox("Dynamic tooltip", &(c.dynamic_tooltip));
				if(!c.dynamic_tooltip) {
					ImGui::InputText("Tooltip key", &(c.tooltip_text_key));
				}
				ImGui::Checkbox("Can be disabled", &(c.can_disable));
				ImGui::Checkbox("Left-click action", &(c.left_click_action));
				ImGui::Checkbox("Right-click action", &(c.right_click_action));
				ImGui::Checkbox("Shift+left-click action", &(c.shift_click_action));
				if(c.left_click_action || c.right_click_action || c.shift_click_action) {
					ImGui::InputText("Hotkey", &(c.hotkey));
				}
				ImGui::Checkbox("Hover activation", &(c.hover_activation));
			}

			ImGui::Checkbox("Ignore rtl flip", &(c.ignore_rtl));

			if(is_lua_element(c)) {
				ImGui::EndDisabled();
			}

		}break;
	}

	ImGui::Text("Member Variables");
	if(c.members.empty()) {
		ImGui::Text("[none]");
	} else {
		for(uint32_t k = 0; k < c.members.size(); ++k) {
			ImGui::PushID(int32_t(k));
			ImGui::Indent();
			ImGui::InputText("Type", &(c.members[k].type));
			ImGui::InputText("Name", &(c.members[k].name));
			if(ImGui::Button("Delete")) {
				c.members.erase(c.members.begin() + k);
			}
			ImGui::Unindent();
			ImGui::PopID();
		}
	}
	if(ImGui::Button("Add Member Variable")) {
		c.members.emplace_back();
	}

}

auto base_tree_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesFull;

std::string produce_label(layout_item& m) {
	if(std::holds_alternative< layout_control_t>(m)) {
		auto& i = std::get<layout_control_t>(m);
		return (char const*)(u8"\uED1E control: ") + i.name;
	} else if(std::holds_alternative<layout_window_t>(m)) {
		auto& i = std::get<layout_window_t>(m);
		return (char const*)(u8"\uE8A7 window: ") + i.name;
	} else if(std::holds_alternative<layout_glue_t>(m)) {
		auto& i = std::get<layout_glue_t>(m);
		std::string label = (char const*)(u8"\uE75D glue: ");
		label += glue_names[int32_t(i.type)];
		label += " " + std::to_string(i.amount);
		return label;
	} else if (std::holds_alternative<texture_layer_t>(m)) {
		auto& i = std::get<texture_layer_t>(m);
		std::string label = texture_layer_names[retrieve_texture_layer_type(i.texture_type)];
		label = (char const*)(u8"\uE8B9 texture layer: ") + label + " ";
		label += i.texture;
		return label;
	} else if(std::holds_alternative<generator_t>(m)) {
		auto& i = std::get<generator_t>(m);

		std::string label = (char const*)(u8"\uE8EE generator: ");
		label += i.name;

		return label;
	} else if(std::holds_alternative< sub_layout_t>(m)) {
		auto& i = std::get<sub_layout_t>(m);
		std::string label = "(";
		if (!(i.layout->open_in_ui)) {
			label = (char const*)(u8"\uED41 sub-layout: ") + label;
		} else {
			label = (char const*)(u8"\uED43 sub-layout: ") + label;
		}
		label += std::to_string(i.layout->contents.size()) + ")###SUBLAYOUT";
		return label;
	}
	return "";
}

struct tree_location {
	std::array<size_t, 64> dir {};
	size_t dir_len = 0;
	bool is_root = false;
	int index = 0;
};

bool update_tree_dnd(layout_level_t& root, layout_level_t& layout, std::string& label, tree_location& current_location) {
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
		ImGui::SetDragDropPayload("DND_layout_items", &current_location, sizeof(tree_location));
		ImGui::Text("Move %s", label.c_str());
		ImGui::EndDragDropSource();
	}
	bool result = false;
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_layout_items")) {
			auto from = *(const tree_location*)payload->Data;
			auto source_dir = retrieve_layout(root, from.dir, from.dir_len);

			if (paths_are_the_same(from.dir, from.dir_len, current_location.dir, current_location.dir_len)) {
				int move_to = current_location.index;
				int move_from = from.index;

				while (move_from > move_to) {
					std::swap(layout.contents[move_from - 1], layout.contents[move_from]);
					move_from--;
				}

				while (move_from < move_to) {
					std::swap(layout.contents[move_from + 1], layout.contents[move_from]);
					move_from++;
				}
			} else {
				layout.contents.insert(
					layout.contents.end(),
					std::make_move_iterator(
						source_dir->contents.begin() + from.index
					),
					std::make_move_iterator(
						source_dir->contents.begin() + from.index + 1
					)
				);
				source_dir->contents.erase(source_dir->contents.begin() + from.index);
			}
			result = true;
			current_edit_target = edit_targets::layout_sublayout;
			path_to_selected_layout = {};
		}
		ImGui::EndDragDropTarget();
	}
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::Selectable("Delete")) {
			layout.contents.erase(layout.contents.begin() + current_location.index);
			path_to_selected_layout.clear();
			current_edit_target = edit_targets::layout_sublayout;
			result = true;
			ImGui::EndPopup();
			return true;
		}
		auto& win = open_project.windows[selected_window];
		auto& buffer = win.buffer;
		if (ImGui::Selectable("Move to buffer")) {
			buffer.insert(
				buffer.end(),
				std::make_move_iterator(
					layout.contents.begin() + current_location.index
				),
				std::make_move_iterator(
					layout.contents.begin() + current_location.index + 1
				)
			);
			layout.contents.erase(layout.contents.begin() + current_location.index);
			path_to_selected_layout.clear();
			current_edit_target = edit_targets::layout_sublayout;
			result = true;
			ImGui::EndPopup();
			return true;
		}
		if(std::holds_alternative<sub_layout_t>(layout.contents[current_location.index])) {
			auto& i = std::get<sub_layout_t>(layout.contents[current_location.index]);
			std::string label = "Move " + std::to_string(buffer.size()) + " items from the buffer###MOVE_FROM_BUFFER";
			if (ImGui::Selectable(label.c_str())) {
				i.layout->contents.insert(
					i.layout->contents.end(),
					std::make_move_iterator(
						buffer.begin()
					),
					std::make_move_iterator(
						buffer.end()
					)
				);
				buffer.clear();
				path_to_selected_layout.clear();
				current_edit_target = edit_targets::layout_sublayout;
				result = true;
			}
			if (ImGui::BeginMenu("Add")) {
				if (ImGui::MenuItem("Control")) {
					i.layout->contents.emplace_back(layout_control_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				if (ImGui::MenuItem("Window")) {
					i.layout->contents.emplace_back(layout_window_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				if (ImGui::MenuItem("Glue")) {
					i.layout->contents.emplace_back(layout_glue_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				if (ImGui::MenuItem("Generator")) {
					i.layout->contents.emplace_back(generator_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				if (ImGui::MenuItem("Sublayout")) {
					i.layout->contents.emplace_back(sub_layout_t{ });
					std::get<sub_layout_t>(i.layout->contents.back()).layout = std::make_unique<layout_level_t>();
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				if (ImGui::MenuItem("Texture layer")) {
					i.layout->contents.emplace_back(texture_layer_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
					result = true;
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndPopup();
	}

	return result;
}

void imgui_layout_contents(layout_level_t& root, layout_level_t& layout, std::vector<size_t> path) {
	bool root_expanded = true;
	if (path.size() == 0) {
		root_expanded = ImGui::TreeNodeEx((char const*)(u8"\uEC4E"), base_tree_flags);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			path_to_selected_layout = {};
			current_edit_target = edit_targets::layout_sublayout;
		}
		if (ImGui::BeginPopupContextItem()) {
			auto& win = open_project.windows[selected_window];
			auto& buffer = win.buffer;
			std::string label = "Move " + std::to_string(buffer.size()) + " items from the buffer###MOVE_FROM_BUFFER";
			if (ImGui::Selectable(label.c_str())) {
				layout.contents.insert(
					layout.contents.end(),
					std::make_move_iterator(
						buffer.begin()
					),
					std::make_move_iterator(
						buffer.end()
					)
				);
				buffer.clear();
				path_to_selected_layout.clear();
				current_edit_target = edit_targets::layout_sublayout;
			}
			if (ImGui::BeginMenu("Add")) {
				if (ImGui::MenuItem("Control")) {
					layout.contents.emplace_back(layout_control_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				if (ImGui::MenuItem("Window")) {
					layout.contents.emplace_back(layout_window_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				if (ImGui::MenuItem("Glue")) {
					layout.contents.emplace_back(layout_glue_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				if (ImGui::MenuItem("Generator")) {
					layout.contents.emplace_back(generator_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				if (ImGui::MenuItem("Sublayout")) {
					layout.contents.emplace_back(sub_layout_t{ });
					std::get<sub_layout_t>(layout.contents.back()).layout = std::make_unique<layout_level_t>();
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				if (ImGui::MenuItem("Texture layer")) {
					layout.contents.emplace_back(texture_layer_t{ });
					path_to_selected_layout.clear();
					current_edit_target = edit_targets::layout_sublayout;
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PushID(&layout);

	int32_t temp = 0;
	int32_t id = 0;
	for(size_t j = 0; j < layout.contents.size() && root_expanded; j++) {
		auto& m = layout.contents[j];

		ImGui::PushID(id);

		auto label = produce_label(m);
		auto flag_mod = paths_are_the_same(path, j, path_to_selected_layout) ? ImGuiTreeNodeFlags_Selected : 0;

		tree_location current_location {
			.dir {},
			.dir_len = path.size(),
			.is_root = false,
			.index = int(j)
		};
		for (auto i = 0; i < path.size(); i++) {
			current_location.dir[i] = path[i];
		}

		if(std::holds_alternative< layout_control_t>(m)) {
			auto& i = std::get<layout_control_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod | ImGuiTreeNodeFlags_Leaf);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_control;
			}
			if (node_open) {
				ImGui::TreePop();
			}
		} else if(std::holds_alternative<layout_window_t>(m)) {
			auto& i = std::get<layout_window_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod | ImGuiTreeNodeFlags_Leaf);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_window;
			}
			if (node_open) {
				ImGui::TreePop();
			}
		} else if(std::holds_alternative<layout_glue_t>(m)) {
			auto& i = std::get<layout_glue_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod | ImGuiTreeNodeFlags_Leaf);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_glue;
			}
			if (node_open) {
				ImGui::TreePop();
			}
		} else if (std::holds_alternative<texture_layer_t>(m)) {
			auto& i = std::get<texture_layer_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod | ImGuiTreeNodeFlags_Leaf);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_texture;
			}
			if (node_open) {
				ImGui::TreePop();
			}
		} else if(std::holds_alternative<generator_t>(m)) {
			auto& i = std::get<generator_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod | ImGuiTreeNodeFlags_Leaf);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_generator;
			}
			if (node_open) {
				ImGui::TreePop();
			}
		} else if(std::holds_alternative< sub_layout_t>(m)) {
			auto& i = std::get<sub_layout_t>(m);
			auto node_open = ImGui::TreeNodeEx(label.c_str(), base_tree_flags | flag_mod);
			if (update_tree_dnd(root, layout, label, current_location)) {if (node_open) {ImGui::TreePop();} ImGui::PopID(); break;}
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				path_to_selected_layout = path;
				path_to_selected_layout.push_back(j);
				current_edit_target = edit_targets::layout_sublayout;
			}

			if(node_open) {
				i.layout->open_in_ui = true;
				auto new_path = path;
				new_path.push_back(j);
				imgui_layout_contents(root, *i.layout, new_path);
				ImGui::TreePop();
			} else {
				i.layout->open_in_ui = false;
			}
		}

		// ImGui::NewLine();

		ImGui::PopID();
		++id;
	}
	ImGui::PopID();

	if (path.size() == 0 && root_expanded) {ImGui::TreePop();}
}

layout_control_t* find_control(layout_level_t& lvl, int32_t index) {
	for(auto& m : lvl.contents) {
		if(std::holds_alternative<layout_control_t>(m)) {
			auto& i = get<layout_control_t>(m);
			if(i.cached_index == int16_t(index))
				return &i;
		} else if(holds_alternative<sub_layout_t>(m)) {
			auto& i = get<sub_layout_t>(m);
			auto r = find_control(*i.layout, index);
			if(r)
				return r;
		}
	}
	return nullptr;
}

// Main code
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	glfwSetErrorCallback(glfw_error_callback);
	if(!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Alice UI Editor", nullptr, nullptr);
	glfwSetScrollCallback(window, scroll_callback);
	glfwMaximizeWindow(window);

	if(window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);
	assert(glewInit() == 0);
	glfwSwapInterval(1); // Enable vsync

	load_global_squares();
	load_shaders();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_SYSTEM_DPI_AWARE);

	POINT ptZero = { 0, 0 };
	auto def_monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
	UINT xdpi = 0;
	UINT ydpi = 0;
	GetDpiForMonitor(def_monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
	auto scale_value = xdpi != 0 ? float(xdpi) / 96.0f : 1.0f;
	auto text_scale = scale_value;
	HKEY reg_text_key = NULL;
	if(RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Accessibility", 0, KEY_NOTIFY | KEY_QUERY_VALUE, &reg_text_key) == ERROR_SUCCESS) {
		DWORD scale = 0;
		DWORD cb = sizeof scale;
		RegQueryValueExW(reg_text_key, L"TextScaleFactor", NULL, NULL, (LPBYTE)&scale, &cb);
		if(scale != 0) {
			text_scale *= float(scale) / 100.0f;
		}
		RegCloseKey(reg_text_key);
	}
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", text_scale * 18.0f);
	ImFontConfig config;
	config.MergeMode = true;
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segmdl2.ttf", text_scale * 12.0f, &config);
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiStyle styleold = style; // Backup colors
	style = ImGuiStyle(); // IMPORTANT: ScaleAllSizes will change the original size, so we should reset all style config
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 1.0f;
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;

	style.WindowRounding = 6.f;
	style.ChildRounding = 6.f;
	style.FrameRounding = 6.f;
	style.PopupRounding = 6.f;
	style.ScrollbarRounding = 6.f;
	style.GrabRounding = 2.f;
	style.TabRounding = 6.f;
	style.SeparatorTextBorderSize = 5.f;
	style.FrameBorderSize = 1.f;
	style.TabBorderSize = 1.f;


	//style.ScaleAllSizes(scale_value);
	CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	float ui_scale = std::max(1.0f, std::floor(text_scale + 0.5f));

	float drag_start_x = 0.0f;
	float drag_start_y = 0.0f;
	struct {
		int32_t x_pos;
		int32_t y_pos;
		int32_t x_size;
		int32_t y_size;
	} base_values;
	bool dragging = false;


	open_project.grid_size = 10;

	drag_target control_drag_target = drag_target::none;

	int32_t display_w = 0;
	int32_t display_h = 0;
	auto switch_to_window = [&](int32_t i) {
		path_to_selected_layout.clear();
		current_edit_target = edit_targets::layout_sublayout;
		if(0 <= i && i < int32_t(open_project.windows.size())) {
			selected_window = i;
			selected_control = -1;
			selected_table = -1;
			drag_offset_x = display_w / 2.0f - open_project.windows[i].wrapped.x_pos * ui_scale;
			drag_offset_y = display_h / 2.0f - open_project.windows[i].wrapped.y_pos * ui_scale - open_project.windows[i].wrapped.y_size * ui_scale / 2.0f;
			chosen_window = i;
			just_chose_window = true;
		} else {
			selected_window = -1;
			selected_control = -1;
			selected_table = -1;
		}
	};

	// Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//if(show_demo_window)
		//	ImGui::ShowDemoWindow();

		bool open_modal_properties = false;
		bool open_modal_table_properties = false;

		if (ImGui::BeginMainMenuBar()) {
			// std::string pname = fs::native_to_utf8(open_project.project_name);

			if (ImGui::BeginMenu("Project")) {
				// ImGui::MenuItem(pname.c_str());

				if (ImGui::MenuItem("New")) {
					auto new_file = fs::pick_new_file(L"aui");
					if(new_file.length() > 0) {
						auto breakpt = new_file.find_last_of(L'\\');
						auto rem = new_file.substr(breakpt + 1);
						auto ext_pos = rem.find_last_of(L'.');
						open_project.project_name = rem.substr(0, ext_pos);
						open_project.project_directory = new_file.substr(0, breakpt + 1);
						switch_to_window(-1);
					}
				}

				if (ImGui::MenuItem("Open", "Ctrl+O")) {
					auto new_file = fs::pick_existing_file(L"aui");
					if(new_file.length() > 0) {
						auto breakpt = new_file.find_last_of(L'\\');
						auto rem = new_file.substr(breakpt + 1);
						auto ext_pos = rem.find_last_of(L'.');
						{
							fs::file loaded_file{ new_file };
							serialization::in_buffer file_content{ loaded_file.content().data, loaded_file.content().file_size };
							open_project = bytes_to_project(file_content);
							open_project.project_name = rem.substr(0, ext_pos);
							open_project.project_directory = new_file.substr(0, breakpt + 1);
							switch_to_window(0);
						}

						{
							fs::file loaded_file{ open_project.project_directory + L"the.tui" };
							serialization::in_buffer file_content{ loaded_file.content().data, loaded_file.content().file_size };
							open_templates = template_project::bytes_to_project(file_content);
							open_templates.project_name = rem.substr(0, ext_pos);
							open_templates.project_directory = open_project.project_directory;
							asvg::common_file_bank::bank.root_directory = open_templates.project_directory + open_templates.svg_directory;

							for(auto& i : open_templates.icons) {
								fs::file loaded_file{ open_templates.project_directory + open_templates.svg_directory + fs::utf8_to_native(i.file_name) };
								i.renders = asvg::simple_svg(loaded_file.content().data, size_t(loaded_file.content().file_size));
							}
							for(auto& b : open_templates.backgrounds) {
								fs::file loaded_file{ open_templates.project_directory + open_templates.svg_directory + fs::utf8_to_native(b.file_name) };
								b.renders = asvg::svg(loaded_file.content().data, size_t(loaded_file.content().file_size), b.base_x, b.base_y);
							}
						}
					}
				}

				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					serialization::out_buffer bytes;
					project_to_bytes(open_project, bytes);
					fs::write_file(open_project.project_directory + open_project.project_name + L".aui", bytes.data(), uint32_t(bytes.size()));
				}

				if (ImGui::MenuItem("Generate code")) {
					generator::code_snippets old_code;
					{
						fs::file loaded_file{ open_project.project_directory + open_project.source_path + open_project.project_name + L".cpp" };
						old_code = generator::extract_snippets(loaded_file.content().data, loaded_file.content().file_size);
					}
					auto text = generator::generate_project_code(open_project, old_code);
					fs::write_file(open_project.project_directory + open_project.source_path + open_project.project_name + L".cpp", text.c_str(), uint32_t(text.size()));
				}

				if (ImGui::MenuItem("Properties"))
					open_modal_properties = true;

				ImGui::EndMenu();
			}


			if (ImGui::BeginMenu("Create")) {
				if (ImGui::MenuItem("Window")) {
					open_project.windows.emplace_back();
					open_project.windows.back().wrapped.x_size = 100;
					open_project.windows.back().wrapped.y_size = 100;
					switch_to_window(int32_t(open_project.windows.size() - 1));
					auto i = open_project.windows.size();
					do {
						std::string def_name = "Window" + std::to_string(i);
						bool found = false;
						for(auto& w : open_project.windows) {
							if(w.wrapped.name == def_name) {
								found = true;
								break;
							}
						}
						if(!found) {
							open_project.windows.back().wrapped.name = def_name;
							break;
						}
						++i;
					} while(true);
				}

				if (ImGui::MenuItem("Table")) {
					open_project.tables.emplace_back();
					auto i = open_project.tables.size();
					do {
						std::string def_name = "Table" + std::to_string(i);
						bool found = false;
						for(auto& w : open_project.tables) {
							if(w.name == def_name) {
								found = true;
								break;
							}
						}
						if(!found) {
							open_project.tables.back().name = def_name;
							break;
						}
						++i;
					} while(true);
				}

				if(0 <= selected_window && selected_window <= int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];
					if (ImGui::MenuItem("Control")) {
						win.children.emplace_back();
						win.children.back().x_pos = int16_t(open_project.grid_size);
						win.children.back().y_pos = int16_t(open_project.grid_size);
						win.children.back().x_size = int16_t(open_project.grid_size * 8);
						win.children.back().y_size = int16_t(open_project.grid_size * 2);

						auto i = win.children.size();
						do {
							std::string def_name = "Control" + std::to_string(i);
							bool found = false;
							for(auto& w : win.children) {
								if(w.name == def_name) {
									found = true;
									break;
								}
							}
							if(!found) {
								win.children.back().name = def_name;
								break;
							}
							++i;
						} while(true);
					}
				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		if (open_modal_properties) {
			ImGui::OpenPopup("Project properties");
		}

		bool is_open = true;
		if (ImGui::BeginPopupModal("Project properties", &is_open, ImGuiWindowFlags_AlwaysAutoResize)){
			static float f = 0.0f;
			static int counter = 0;

			if(open_project.project_name.empty() == false) {
				auto asssets_location = std::string("Source directory: ") + (open_project.source_path.empty() ? std::string("[none]") : fs::native_to_utf8(open_project.source_path));
				ImGui::Text(asssets_location.c_str());
				ImGui::SameLine();
				if(ImGui::Button("Change")) {
					auto new_dir = fs::pick_directory(open_project.project_directory) + L"\\";
					if(new_dir.length() > 1) {
						size_t common_length = 0;
						while(common_length < new_dir.size()) {
							auto next_common_length = new_dir.find_first_of(L'\\', common_length);
							if(new_dir.substr(0, next_common_length + 1) != open_project.project_directory.substr(0, next_common_length + 1)) {
								break;
							}
							common_length = next_common_length + size_t(1);
						}
						uint32_t missing_separators_count = 0;
						for(size_t i = common_length; i < open_project.project_directory.size(); ++i) {
							if(open_project.project_directory[i] == L'\\') {
								++missing_separators_count;
							}
						}
						if(missing_separators_count == 0) {
							if(common_length >= new_dir.size()) {
								open_project.source_path = L".\\";
							} else {
								open_project.source_path = new_dir.substr(common_length);
							}
						} else {
							std::wstring temp;
							for(uint32_t i = 0; i < missing_separators_count; ++i) {
								temp += L"..\\";
							}
							if(common_length >= new_dir.size()) {
								open_project.source_path = temp;
							} else {
								open_project.source_path = temp + new_dir.substr(common_length);
							}
						}

						for(auto& win : open_project.windows) {
							win.wrapped.ogl_texture.unload();
							for(auto& c : win.children) {
								c.ogl_texture.unload();
							}
						}
					}
				}
				ImGui::InputInt("Grid size (it's basically a font size!!! don't set to 1)", &open_project.grid_size);
				if(open_project.grid_size < 1) {
					open_project.grid_size = 1;
				}
			}
			ImGui::End();
		}

		if(open_project.project_name.empty() == false && selected_window >= 0) {
			ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

			ImGui::Begin("Edit", NULL, ImGuiWindowFlags_MenuBar);

			if (current_edit_target == edit_targets::window) {
				if(ImGui::Button("Delete")) {
					open_project.windows.erase(open_project.windows.begin() + selected_window);
					switch_to_window(-1);
				} else {
					ImGui::SameLine();
					if(ImGui::Button("Copy container")) {
						open_project.windows.push_back(open_project.windows[selected_window]);
						open_project.windows.back().wrapped.name += "_copy";
					}
					ImGui::InputText("Name", &(open_project.windows[selected_window].wrapped.temp_name));
					ImGui::SameLine();
					if(ImGui::Button("Change Name")) {
						if(open_project.windows[selected_window].wrapped.temp_name.empty()) {
							MessageBoxW(nullptr, L"Name cannot be empty", L"Invalid Name", MB_OK);
						} else {
							bool found = false;
							for(auto& w : open_project.windows) {
								if(w.wrapped.name == open_project.windows[selected_window].wrapped.temp_name) {
									found = true;
									break;
								}
							}
							if(found) {
								MessageBoxW(nullptr, L"Name must be unique", L"Invalid Name", MB_OK);
							} else {
								for(auto& ow : open_project.windows)
									rename_window(ow.layout, open_project.windows[selected_window].wrapped.name, open_project.windows[selected_window].wrapped.temp_name);

								open_project.windows[selected_window].wrapped.name = open_project.windows[selected_window].wrapped.temp_name;
							}
						}
					}
					ImGui::InputText("Parent", &(open_project.windows[selected_window].wrapped.parent));

					int32_t temp = open_project.windows[selected_window].wrapped.x_pos;
					ImGui::InputInt("X position", &temp);
					open_project.windows[selected_window].wrapped.x_pos = int16_t(temp);

					temp = open_project.windows[selected_window].wrapped.y_pos;
					ImGui::InputInt("Y position", &temp);
					open_project.windows[selected_window].wrapped.y_pos = int16_t(temp);

					temp = open_project.windows[selected_window].wrapped.x_size;
					ImGui::InputInt("Width", &temp);
					open_project.windows[selected_window].wrapped.x_size = int16_t(std::max(0, temp));

					temp = open_project.windows[selected_window].wrapped.y_size;
					ImGui::InputInt("Height", &temp);
					open_project.windows[selected_window].wrapped.y_size = int16_t(std::max(0, temp));

					ImVec4 ccolor{ open_project.windows[selected_window].wrapped.rectangle_color.r, open_project.windows[selected_window].wrapped.rectangle_color.b, open_project.windows[selected_window].wrapped.rectangle_color.g, 1.0f };
					ImGui::ColorEdit3("Outline color", (float*)&ccolor);
					open_project.windows[selected_window].wrapped.rectangle_color.r = ccolor.x;
					open_project.windows[selected_window].wrapped.rectangle_color.g = ccolor.z;
					open_project.windows[selected_window].wrapped.rectangle_color.b = ccolor.y;

					ImGui::Checkbox("Ignore grid", &(open_project.windows[selected_window].wrapped.no_grid));

					{
						int32_t combo_selection = open_project.windows[selected_window].wrapped.template_id + 1;
						std::vector<char const*> options;
						options.push_back("--None--");

						for(auto& c : open_templates.window_t) {
							options.push_back(c.display_name.c_str());
						}

						if(ImGui::Combo("Window template", &combo_selection, options.data(), int32_t(options.size()))) {
							open_project.windows[selected_window].wrapped.template_id = combo_selection - 1;
						}
					}

					auto i = selected_window;

					if(open_project.windows[selected_window].wrapped.template_id != -1) {
						if(open_templates.window_t[open_project.windows[selected_window].wrapped.template_id].close_button_definition != -1)
							ImGui::Checkbox("Add close button", &(open_project.windows[selected_window].wrapped.auto_close_button));
					}

					if(open_project.windows[selected_window].wrapped.template_id == -1) {
						const char* items[] = { "none", "texture", "bordered texture", "legacy GFX" };
						temp = int32_t(open_project.windows[i].wrapped.background);
						ImGui::Combo("Background", &temp, items, 4);
						open_project.windows[i].wrapped.background = background_type(temp);
					}

					if(open_project.windows[selected_window].wrapped.template_id == -1) {
						if(open_project.windows[selected_window].wrapped.background == background_type::existing_gfx) {
							ImGui::InputText("Texture", &(open_project.windows[i].wrapped.texture));
						} else if(open_project.windows[i].wrapped.background != background_type::none) {
							std::string tex = "Texture: " + (open_project.windows[i].wrapped.texture.size() > 0 ? open_project.windows[i].wrapped.texture : std::string("[none]"));
							ImGui::Text(tex.c_str());
							ImGui::SameLine();
							if(ImGui::Button("Change")) {
								auto new_file = fs::pick_existing_file(L"");
								//auto breakpt = new_file.find_last_of(L'\\');
								//open_project.windows[i].wrapped.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
								open_project.windows[i].wrapped.texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
								open_project.windows[i].wrapped.ogl_texture.unload();
							}
							ImGui::Checkbox("Has alternate background", &(open_project.windows[i].wrapped.has_alternate_bg));
							if(open_project.windows[selected_window].wrapped.has_alternate_bg) {
								std::string tex = "Alternate texture: " + (open_project.windows[i].wrapped.alternate_bg.size() > 0 ? open_project.windows[i].wrapped.alternate_bg : std::string("[none]"));
								ImGui::Text(tex.c_str());
								ImGui::SameLine();
								if(ImGui::Button("Change Alternate")) {
									auto new_file = fs::pick_existing_file(L"");
									//auto breakpt = new_file.find_last_of(L'\\');
									//open_project.windows[i].wrapped.alternate_bg = fs::native_to_utf8(new_file.substr(breakpt + 1));
									open_project.windows[i].wrapped.alternate_bg = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
								}
							}
						}
						if(open_project.windows[selected_window].wrapped.background == background_type::bordered_texture) {
							temp = open_project.windows[selected_window].wrapped.border_size;
							ImGui::InputInt("Border size", &temp);
							open_project.windows[selected_window].wrapped.border_size = int16_t(std::max(0, temp));
						}
					}
					ImGui::Checkbox("Share table highlight", &(open_project.windows[selected_window].wrapped.share_table_highlight));
					if(open_project.windows[i].wrapped.share_table_highlight) {
						std::vector<char const*> table_names;
						table_names.push_back("[none]");
						int32_t selection = (open_project.windows[selected_window].wrapped.table_connection== "" ? 0 : -1);
						for(auto& c : open_project.tables) {
							table_names.push_back(c.name.c_str());
							if(c.name == open_project.windows[selected_window].wrapped.table_connection) {
								selection = int32_t(table_names.size() - 1);
							}
						}

						temp = selection;
						ImGui::Combo("Associated table", &selection, table_names.data(), int32_t(table_names.size()));
						if(temp != selection) {
							if(selection == 0)
								open_project.windows[i].wrapped.table_connection = "";
							else
								open_project.windows[i].wrapped.table_connection = open_project.tables[selection - 1].name;
						}
					}
					ImGui::Checkbox("Receive updates while hidden", &(open_project.windows[i].wrapped.updates_while_hidden));
					ImGui::Checkbox("On hide/close action", &(open_project.windows[i].wrapped.on_hide_action));
					temp = 0;
					switch(open_project.windows[i].wrapped.orientation) {
						case orientation::upper_left: temp = 0; break;
						case orientation::upper_right: temp = 1; break;
						case orientation::lower_left: temp = 2; break;
						case orientation::lower_right: temp = 3; break;
						case orientation::upper_center: temp = 4; break;
						case orientation::lower_center: temp = 5; break;
						case orientation::center: temp = 6; break;
					}
					{
						const char* items[] = { "upper left", "upper right", "lower left", "lower right", "upper center", "lower center", "center" };
						ImGui::Combo("Position in parent", &temp, items, 7);
					}
					switch(temp) {
						case 0: open_project.windows[i].wrapped.orientation = orientation::upper_left; break;
						case 1: open_project.windows[i].wrapped.orientation = orientation::upper_right; break;
						case 2: open_project.windows[i].wrapped.orientation = orientation::lower_left; break;
						case 3: open_project.windows[i].wrapped.orientation = orientation::lower_right; break;
						case 4: open_project.windows[i].wrapped.orientation = orientation::upper_center; break;
						case 5: open_project.windows[i].wrapped.orientation = orientation::lower_center; break;
						case 6: open_project.windows[i].wrapped.orientation = orientation::center; break;
					}
					ImGui::Checkbox("Ignore rtl flip", &(open_project.windows[i].wrapped.ignore_rtl));
					ImGui::Checkbox("Draggable", &(open_project.windows[i].wrapped.draggable));

					ImGui::Text("Member Variables");
					if(open_project.windows[i].wrapped.members.empty()) {
						ImGui::Text("[none]");
					} else {
						for(uint32_t k = 0; k < open_project.windows[i].wrapped.members.size(); ++k) {
							ImGui::PushID(int32_t(k));
							ImGui::Indent();
							ImGui::InputText("Type", &(open_project.windows[i].wrapped.members[k].type));
							ImGui::InputText("Name", &(open_project.windows[i].wrapped.members[k].name));
							if(ImGui::Button("Delete")) {
								open_project.windows[i].wrapped.members.erase(open_project.windows[i].wrapped.members.begin() + k);
							}
							ImGui::Unindent();
							ImGui::PopID();
						}
					}
					if(ImGui::Button("Add Member Variable")) {
						open_project.windows[i].wrapped.members.emplace_back();
					}

					if(open_project.windows[selected_window].wrapped.template_id != -1) {
						bool has_alt_list = (open_project.windows[selected_window].alternates.empty() == false);
						if(ImGui::Checkbox("Has alternate template set", &has_alt_list)) {
							if(has_alt_list && open_project.windows[selected_window].alternates.empty()) {
								open_project.windows[selected_window].alternates.push_back(template_alternate{ "", -1 });
							}
							if(!has_alt_list) {
								open_project.windows[selected_window].alternates.clear();
							}
						}
						if(has_alt_list) {
							auto get_alt_id = [&](std::string_view s) {
								for(auto& alt : open_project.windows[selected_window].alternates) {
									if(alt.control_name == s)
										return alt.tempalte_id;
								}
								return -1;
							};
							auto set_alt_id = [&](std::string_view s, int32_t id) {
								for(auto& alt : open_project.windows[selected_window].alternates) {
									if(alt.control_name == s) {
										alt.tempalte_id = id;
										return;
									}
								}
								open_project.windows[selected_window].alternates.push_back(template_alternate{ std::string(s), id });
							};
							{
								std::vector<char const*> inner_opts;
								inner_opts.push_back("--Don't change--");
								for(auto& i : open_templates.window_t) {
									inner_opts.push_back(i.display_name.c_str());
								}
								int32_t chosen = get_alt_id("") + 1;
								if(ImGui::Combo("Alternate window template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
									set_alt_id("", chosen - 1);
								}
							}
							for(auto& c : open_project.windows[selected_window].children) {
								switch(c.ttype) {
									case template_project::template_type::none:
										break;
									case template_project::template_type::label:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.label_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::button:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.button_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::toggle_button:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.toggle_button_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::iconic_button:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.iconic_button_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::mixed_button:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.mixed_button_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::mixed_button_ci:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.mixed_button_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::free_icon:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.icons) {
											inner_opts.push_back(i.file_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::free_background:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.backgrounds) {
											inner_opts.push_back(i.file_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
									case template_project::template_type::progress_bar:
									{
										std::vector<char const*> inner_opts;
										inner_opts.push_back("--Don't change--");
										for(auto& i : open_templates.progress_bar_t) {
											inner_opts.push_back(i.display_name.c_str());
										}
										int32_t chosen = get_alt_id(c.name) + 1;
										std::string label = "Alternate template for " + c.name;
										if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
											set_alt_id(c.name, chosen - 1);
										}
									} break;
								}
							}
						}
					}

					if(open_project.windows[selected_window].wrapped.template_id == -1) {
						std::string tex = "Page left: " + (open_project.windows[i].wrapped.page_left_texture.size() > 0 ? open_project.windows[i].wrapped.page_left_texture : std::string("[none]"));
						ImGui::Text(tex.c_str());
						ImGui::SameLine();
						if(ImGui::Button("Change (pl)")) {
							auto new_file = fs::pick_existing_file(L"");
							//auto breakpt = new_file.find_last_of(L'\\');
							//open_project.windows[i].wrapped.page_left_texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
							open_project.windows[i].wrapped.page_left_texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
						}
					}
					if(open_project.windows[selected_window].wrapped.template_id == -1) {
						std::string tex = "Page right: " + (open_project.windows[i].wrapped.page_right_texture.size() > 0 ? open_project.windows[i].wrapped.page_right_texture : std::string("[none]"));
						ImGui::Text(tex.c_str());
						ImGui::SameLine();
						if(ImGui::Button("Change (pr)")) {
							auto new_file = fs::pick_existing_file(L"");
							//auto breakpt = new_file.find_last_of(L'\\');
							//open_project.windows[i].wrapped.page_right_texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
							open_project.windows[i].wrapped.page_right_texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
						}
					}
					if(open_project.windows[selected_window].wrapped.template_id == -1) {
						const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
						temp = int32_t(open_project.windows[i].wrapped.page_text_color);
						ImGui::Combo("Page number text color", &temp, items, 16);
						open_project.windows[i].wrapped.page_text_color = text_color(temp);
					}
				}
			}

			if (
				current_edit_target == edit_targets::control
				&& selected_control > -1
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto& c = win.children[selected_control];

				control_options(win, c);

			}

			if (
				current_edit_target == edit_targets::layout_window
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto current_item = &win.layout;
				for (size_t i = 0; i + 1 < path_to_selected_layout.size(); i++) {
					current_item = std::get<sub_layout_t>(current_item->contents[path_to_selected_layout[i]]).layout.get();
				}

				auto& i = std::get<layout_window_t>(current_item->contents[path_to_selected_layout.back()]);
				int32_t temp;

				std::vector<char const*> window_names;
				window_names.push_back("[none]");
				int32_t selection = (i.name == "" ? 0 : -1);
				for(auto& win : open_project.windows) {
					window_names.push_back(win.wrapped.name.c_str());
					if(win.wrapped.name == i.name) {
						selection = int32_t(window_names.size() - 1);
					}
				}

				temp = selection;
				ImGui::Combo("Name", &selection, window_names.data(), int32_t(window_names.size()));
				if(temp != selection && selection != selected_window) {
					if(selection == 0)
						i.name = "";
					else
						i.name = open_project.windows[selection - 1].wrapped.name;
				}

				ImGui::Checkbox("Absolute position", &(i.absolute_position));
				if(i.absolute_position) {
					temp = i.abs_x;
					ImGui::InputInt("Absolute x position", &temp);
					i.abs_x = int16_t(temp);

					temp = i.abs_y;
					ImGui::InputInt("Absolute y position", &temp);
					i.abs_y = int16_t(temp);
				}
			}

			if (
				current_edit_target == edit_targets::layout_generator
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto current_item = &win.layout;
				for (size_t i = 0; i + 1 < path_to_selected_layout.size(); i++) {
					current_item = std::get<sub_layout_t>(current_item->contents[path_to_selected_layout[i]]).layout.get();
				}

				auto& i = std::get<generator_t>(current_item->contents[path_to_selected_layout.back()]);
				int32_t temp;

				ImGui::InputText("Name", &(i.name));

				ImGui::Text("Generated items:");
				int32_t id2 = 4000;
				for(auto& win : open_project.windows) {
					if(win.wrapped.name == open_project.windows[selected_window].wrapped.name)
						continue;

					ImGui::PushID(id2);
					auto list_it = std::find_if(i.inserts.begin(), i.inserts.end(), [&](auto& p) { return p.name == win.wrapped.name; });
					bool already_in_list = list_it != i.inserts.end();
					bool temp_option = already_in_list;

					ImGui::Checkbox(win.wrapped.name.c_str(), &temp_option);

					if(temp_option && !already_in_list) {
						i.inserts.push_back(generator_item{ win.wrapped.name, "", -1, false });
					} else if(!temp_option && already_in_list) {
						i.inserts.erase(list_it);
						ImGui::PopID();
						break;
					}
					if(already_in_list && temp_option) {
						ImGui::Indent();

						temp = list_it->inter_item_space;
						ImGui::InputInt("Inter-item space", &temp);
						list_it->inter_item_space = int16_t(temp);

						{
							const char* items[] = { "standard", "at least", "line break", "page break", "don't break" };
							temp = int32_t(list_it->glue);
							ImGui::Combo("Glue type", &temp, items, 5);
							list_it->glue = glue_type(temp);
						}

						ImGui::Checkbox("Sortable", &(list_it->sortable));

						std::vector<char const*> window_names;
						window_names.push_back("[none]");
						int32_t selection = (list_it->header == "" ? 0 : -1);
						for(auto& win : open_project.windows) {
							window_names.push_back(win.wrapped.name.c_str());
							if(win.wrapped.name == list_it->header) {
								selection = int32_t(window_names.size() - 1);
							}
						}

						temp = selection;
						ImGui::Combo("Header", &selection, window_names.data(), int32_t(window_names.size()));
						if(temp != selection && selection != selected_window) {
							if(selection == 0)
								list_it->header = "";
							else
								list_it->header = open_project.windows[selection - 1].wrapped.name;
						}

						// ImGui::Unindent();
					}
					ImGui::PopID();
					++id2;
				}
			}

			if (
				current_edit_target == edit_targets::layout_texture
			) {
				auto& win = open_project.windows[selected_window];
				auto current_item = &win.layout;
				for (size_t i = 0; i + 1 < path_to_selected_layout.size(); i++) {
					current_item = std::get<sub_layout_t>(current_item->contents[path_to_selected_layout[i]]).layout.get();
				}

				auto& i = std::get<texture_layer_t>(current_item->contents[path_to_selected_layout.back()]);

				{
					int index = retrieve_texture_layer_type(i.texture_type);
					ImGui::Combo(
						"Texture type",
						&index,
						texture_layer_names,
						4
					);
					i.texture_type = texture_layer_available_types[index];
				}

				{
					std::string tex = "Texture: " + (i.texture.size() > 0 ? i.texture : std::string("[none]"));
					ImGui::Text(tex.c_str());
					ImGui::SameLine();
					if(ImGui::Button("Change")) {
						auto new_file = fs::pick_existing_file(L"");
						//auto breakpt = new_file.find_last_of(L'\\');
						//open_project.windows[i].wrapped.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
						i.texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					}
				}
			}

			if (
				current_edit_target == edit_targets::layout_glue
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto current_item = &win.layout;
				for (size_t i = 0; i + 1 < path_to_selected_layout.size(); i++) {
					current_item = std::get<sub_layout_t>(current_item->contents[path_to_selected_layout[i]]).layout.get();
				}

				auto& i = std::get<layout_glue_t>(current_item->contents[path_to_selected_layout.back()]);
				int32_t temp;

				{
					temp = int32_t(i.type);
					ImGui::Combo("Glue type", &temp, glue_names, 5);
					i.type = glue_type(temp);
				}
				if(i.type == glue_type::standard || i.type == glue_type::at_least || i.type == glue_type::glue_don_t_break) {
					temp = i.amount;
					ImGui::InputInt("Glue amount", &temp);
					i.amount = int16_t(temp);
				}
			}

			if (
				current_edit_target == edit_targets::layout_control
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto current_item = &win.layout;
				for (size_t i = 0; i + 1 < path_to_selected_layout.size(); i++) {
					current_item = std::get<sub_layout_t>(current_item->contents[path_to_selected_layout[i]]).layout.get();
				}

				auto& selected_item = std::get<layout_control_t>(current_item->contents[path_to_selected_layout.back()]);

				std::vector<char const*> control_names;
				control_names.push_back("[none]");
				int32_t selection = (selected_item.name == "" ? 0 : -1);
				int32_t temp;
				for(auto& c : open_project.windows[selected_window].children) {
					control_names.push_back(c.name.c_str());
					if(c.name == selected_item.name) {
						selection = int32_t(control_names.size() - 1);
					}
				}

				temp = selection;
				ImGui::Combo("Name", &selection, control_names.data(), int32_t(control_names.size()));
				if(temp != selection) {
					if(selection == 0)
						selected_item.name = "";
					else
						selected_item.name = open_project.windows[selected_window].children[selection -1].name;
				}

				ImGui::Checkbox("Absolute position", &(selected_item.absolute_position));
				if(selected_item.absolute_position) {
					temp = selected_item.abs_x;
					ImGui::InputInt("Absolute x position", &temp);
					selected_item.abs_x = int16_t(temp);

					temp = selected_item.abs_y;
					ImGui::InputInt("Absolute y position", &temp);
					selected_item.abs_y = int16_t(temp);
				}

				if (ImGui::Button("Edit control") && selection > 0) {
					selected_control = selection - 1;
					current_edit_target = edit_targets::control;
				}
			}

			if (
				current_edit_target == edit_targets::layout_sublayout
				&& selected_window > -1
				&& selected_window < open_project.windows.size()
			) {
				auto& win = open_project.windows[selected_window];
				auto selected_layout = &win.layout;
				for (auto i : path_to_selected_layout) {
					selected_layout = std::get<sub_layout_t>(selected_layout->contents[i]).layout.get();
				}
				int temp;

				if (ImGui::BeginMenuBar()) {
					if (ImGui::BeginMenu("Add")) {
						if (ImGui::MenuItem("Control")) {
							selected_layout->contents.emplace_back(layout_control_t{ });
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						if (ImGui::MenuItem("Window")) {
							selected_layout->contents.emplace_back(layout_window_t{ });
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						if (ImGui::MenuItem("Glue")) {
							selected_layout->contents.emplace_back(layout_glue_t{ });
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						if (ImGui::MenuItem("Generator")) {
							selected_layout->contents.emplace_back(generator_t{ });
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						if (ImGui::MenuItem("Sublayout")) {
							selected_layout->contents.emplace_back(sub_layout_t{ });
							std::get<sub_layout_t>(selected_layout->contents.back()).layout = std::make_unique<layout_level_t>();
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						if (ImGui::MenuItem("Texture layer")) {
							selected_layout->contents.emplace_back(texture_layer_t{ });
							path_to_selected_layout.clear();
							current_edit_target = edit_targets::layout_sublayout;
						}
						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}

				{
					temp = int32_t(selected_layout->type);
					const char* items[] = { "horizontal", "vertical", "overlapped horizontal", "overlapped vertical", "multiline", "multicolumn" };
					ImGui::Combo("Layout type", &temp, items, 6);
					selected_layout->type = layout_type(temp);
				}

				temp = selected_layout->size_x;
				ImGui::InputInt("Width (-1 for maximal)", &temp);
				selected_layout->size_x = int16_t(temp);

				temp = selected_layout->size_y;
				ImGui::InputInt("Height (-1 for maximal)", &temp);
				selected_layout->size_y = int16_t(temp);

				temp = selected_layout->margin_top;
				ImGui::InputInt("Top margin", &temp);
				selected_layout->margin_top = int16_t(temp);

				temp = selected_layout->margin_bottom;
				ImGui::InputInt("Bottom margin", &temp);
				selected_layout->margin_bottom = int16_t(temp);

				temp = selected_layout->margin_left;
				ImGui::InputInt("Left margin", &temp);
				selected_layout->margin_left = int16_t(temp);

				temp = selected_layout->margin_right;
				ImGui::InputInt("Right margin", &temp);
				selected_layout->margin_right = int16_t(temp);

				{
					std::vector<char const*> inner_opts;
					inner_opts.push_back("Default");
					for(auto& i : open_templates.layout_region_t) {
						inner_opts.push_back(i.display_name.c_str());
					}
					int32_t chosen = selected_layout->template_id + 1;
					if(ImGui::Combo("Template", &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
						selected_layout->template_id = int16_t(chosen - 1);
					}
				}

				{
					const char* items[] = { "leading", "trailing", "centered" };
					temp = int32_t(selected_layout->line_alignment);
					ImGui::Combo("Line alignment", &temp, items, 3);
					selected_layout->line_alignment = layout_line_alignment(temp);
				}
				{
					const char* items[] = { "leading", "trailing", "centered" };
					temp = int32_t(selected_layout->line_internal_alignment);
					ImGui::Combo("Internal line alignment", &temp, items, 3);
					selected_layout->line_internal_alignment = layout_line_alignment(temp);
				}
				if(selected_layout->type == layout_type::mulitline_horizontal || selected_layout->type == layout_type::multiline_vertical) {
					temp = selected_layout->interline_spacing;
					ImGui::InputInt("Interline spacing", &temp);
					selected_layout->interline_spacing = uint8_t(temp);
				}

				ImGui::Checkbox("Paged", &(selected_layout->paged));
				if(selected_layout->paged) {
					{
						const char* items[] = { "none", "page turn (left)", "page turn (right)", "page turn (up)", "page turn (middle)" };
						temp = int32_t(selected_layout->page_animation);
						ImGui::Combo("Animation", &temp, items, 5);
						selected_layout->page_animation = animation_type(temp);
					}
					/* {
						const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
						temp = int32_t(selected_layout->page_display_color);
						ImGui::Combo("Page numbering color", &temp, items, 16);
						selected_layout->page_display_color = text_color(temp);
					} */
				}

			}
			ImGui::End();
		}

		if (open_project.project_name.empty() == false) {
			ImGui::SetNextWindowPos({5 * scale_value, 40 * scale_value});
			ImGui::SetNextWindowSize({scale_value * 300, io.DisplaySize.y - 80 * scale_value});

			ImGui::Begin("Layout", NULL,
				ImGuiWindowFlags_HorizontalScrollbar
				|| ImGuiWindowFlags_NoTitleBar
				|| ImGuiWindowFlags_NoMove
				|| ImGuiWindowFlags_NoResize
			);

			int selection = selected_window + 1;
			// prepare names of the windows

			std::vector<const char*> window_names {"None"};
			for (auto& w : open_project.windows) {
				window_names.push_back(w.wrapped.name.c_str());
			}

			ImGui::Combo("Window", &selection, window_names.data(), window_names.size());

			if (selected_window != selection - 1)
				switch_to_window(selection - 1);

			if (ImGui::Button("Edit window")) {
				current_edit_target = edit_targets::window;
			}

			ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
			if (ImGui::BeginTabBar("SelectionTabs", tab_bar_flags)) {
				if (ImGui::BeginTabItem("Tables")) {
					for(size_t ti = 0; ti < open_project.tables.size(); ++ti) {
						ImGui::PushID(int32_t(ti));
						auto& c = open_project.tables[ti];
						if (ImGui::Button(c.name.c_str())) {
							selected_table = ti;
							open_modal_table_properties = true;
						}
						ImGui::PopID();
					}
					ImGui::EndTabItem();
				}
				if(0 <= selected_window && selected_window <= int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];

					if (ImGui::BeginTabItem("Layout")) {
						imgui_layout_contents(
							open_project.windows[selected_window].layout,
							open_project.windows[selected_window].layout,
							{}
						);
						ImGui::EndTabItem();
					}
					if (win.children.size() > 0 && ImGui::BeginTabItem("Control")) {
						for(uint32_t j = 0; j < win.children.size(); ++j) {
							ImGui::PushID(int32_t(j));

							auto name = win.children[j].name;
							if (hovered_control == int32_t(j)) {
								name += "(Hovered)";
							}

							if (ImGui::RadioButton(name.c_str(), &selected_control, j)) {
								current_edit_target = edit_targets::control;
								selected_control = j;
							}

							ImGui::PopID();
						}
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
			ImGui::End();
		}

		if (open_modal_table_properties) {
			ImGui::OpenPopup("Table properties");
		}

		bool edit_table = true;
		if (ImGui::BeginPopupModal("Table properties", &edit_table, ImGuiWindowFlags_AlwaysAutoResize)) {
			if(ImGui::Button("Delete")) {
				switch_to_window(-1);
				open_project.tables.erase(open_project.tables.begin() + selected_table);
			} else {
				auto& c = open_project.tables[selected_table];

				ImGui::InputText("Name", &(c.temp_name));
				ImGui::SameLine();
				if(ImGui::Button("Change Name")) {
					if(c.temp_name.empty()) {
						MessageBoxW(nullptr, L"Name cannot be empty", L"Invalid Name", MB_OK);
					} else {
						bool found = false;
						for(auto& w : open_project.tables) {
							if(w.name == c.temp_name) {
								found = true;
								break;
							}
						}
						if(found) {
							MessageBoxW(nullptr, L"Name must be unique", L"Invalid Name", MB_OK);
						} else {
							for(auto& ow : open_project.windows) {
								if(ow.wrapped.table_connection == c.name)
									ow.wrapped.table_connection = c.temp_name;
								for(auto& wc : ow.children) {
									if(wc.table_connection == c.name)
										wc.table_connection = c.temp_name;
								}
							}

							c.name = c.temp_name;
						}
					}
				}

				{
					std::vector<char const*> inner_opts;
					inner_opts.push_back("None");
					for(auto& i : open_templates.table_t) {
						inner_opts.push_back(i.display_name.c_str());
					}
					int32_t chosen = c.template_id + 1;
					std::string label = "Template " + c.name;
					if(ImGui::Combo(label.c_str(), &chosen, inner_opts.data(), int32_t(inner_opts.size()))) {
						c.template_id =  chosen - 1;
					}
				}

				if(c.template_id == -1) {
					ImGui::Checkbox("Highlight contents following mouse", &(c.has_highlight_color));
					if(c.has_highlight_color) {
						ImVec4 ccolor{ c.highlight_color.r, c.highlight_color.g, c.highlight_color.b, c.highlight_color.a };
						ImGui::ColorEdit4("Highlight color", (float*)&ccolor);
						c.highlight_color.r = ccolor.x;
						c.highlight_color.g = ccolor.y;
						c.highlight_color.b = ccolor.z;
						c.highlight_color.a = ccolor.w;
					}

					std::string tex = "Ascending sort icon: " + (c.ascending_sort_icon.size() > 0 ? c.ascending_sort_icon : std::string("[none]"));
					ImGui::Text(tex.c_str());
					ImGui::SameLine();
					if(ImGui::Button("Change asc icon")) {
						auto new_file = fs::pick_existing_file(L"");
						//auto breakpt = new_file.find_last_of(L'\\');
						//c.ascending_sort_icon = fs::native_to_utf8(new_file.substr(breakpt + 1));
						c.ascending_sort_icon = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					}

					tex = "Descending sort icon: " + (c.descending_sort_icon.size() > 0 ? c.descending_sort_icon : std::string("[none]"));
					ImGui::Text(tex.c_str());
					ImGui::SameLine();
					if(ImGui::Button("Change des icon")) {
						auto new_file = fs::pick_existing_file(L"");
						//auto breakpt = new_file.find_last_of(L'\\');
						//c.descending_sort_icon = fs::native_to_utf8(new_file.substr(breakpt + 1));
						c.descending_sort_icon = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
					}

					{
						ImVec4 ccolor{ c.divider_color.r, c.divider_color.b, c.divider_color.g, 0.0f };
						ImGui::ColorEdit3("Labels divider color", (float*)&ccolor);
						c.divider_color.r = ccolor.x;
						c.divider_color.g = ccolor.z;
						c.divider_color.b = ccolor.y;
					}
				}

				size_t amount_of_columns = c.table_columns.size();

				int move_from = -1;
				int move_to = -1;

				static ImGuiTableFlags flags =
					ImGuiTableFlags_Borders
					| ImGuiTableFlags_RowBg
					| ImGuiTableFlags_ScrollX
					| ImGuiTableFlags_ScrollY;

				if (ImGui::BeginTable("table_description", amount_of_columns + 1, flags, ImVec2(0.0f, 500.f))) {
					ImGui::TableSetupScrollFreeze(1, 1);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Reorder");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);

						ImGui::PushID(k);
						ImGui::Button("drag");
						if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
						{
							ImGui::SetDragDropPayload("DRAG_TABLE_COLUMN", &k, sizeof(int));
							ImGui::Text("Moving column");
							ImGui::EndDragDropSource();
						}

						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_TABLE_COLUMN"))
							{
								move_from = *(const int*)payload->Data;
								move_to = k;
								if (move_from != -1 && move_to != -1) {
									// Reorder items
									int copy_dst = (move_from > move_to) ? move_to : move_to - 1;
									int copy_src = move_from;

									auto item = c.table_columns[copy_src];
									c.table_columns.erase(c.table_columns.begin() + copy_src);
									c.table_columns.insert(c.table_columns.begin() + copy_dst, item);

									// ImGui::SetDragDropPayload("DRAG_TABLE_COLUMN", &move_to, sizeof(int));
								}
							}
							ImGui::EndDragDropTarget();
						}
						ImGui::PopID();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Column name");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						ImGui::InputText("##Column name", &(c.table_columns[k].internal_data.column_name));
						ImGui::PopID();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Column width");

					int32_t temp;

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);

						ImGui::PushID(k);
						temp = c.table_columns[k].display_data.width;
						ImGui::SetNextItemWidth(100.f);
						ImGui::InputInt("##Column width", &temp);
						c.table_columns[k].display_data.width = int16_t(std::max(0, temp));
						ImGui::PopID();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Spacer column");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);

						ImGui::PushID(k);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						ImGui::SetNextItemWidth(50.f);
						ImGui::Checkbox("##Spacer column", &is_spacer);
						c.table_columns[k].internal_data.cell_type = is_spacer ? table_cell_type::spacer : table_cell_type::text;
						ImGui::PopID();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Cell text color");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k); 

						if(c.template_id == -1) {
							const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
							temp = int32_t(c.table_columns[k].display_data.cell_text_color);
							ImGui::SetNextItemWidth(100.f);
							ImGui::Combo("##Cell text color", &temp, items, 16);
							c.table_columns[k].display_data.cell_text_color = text_color(temp);
						} else {
							int32_t color_choice = int32_t(c.table_columns[k].display_data.cell_text_color);
							make_color_combo_box(color_choice, "##Cell text color", open_templates);
							c.table_columns[k].display_data.cell_text_color = text_color(color_choice);
						}
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Cell text alignment");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						const char* items[] = { "left (leading)", "right (trailing)", "center" };
						temp = int32_t(c.table_columns[k].display_data.text_alignment);
						ImGui::Combo("##Cell text alignment", &temp, items, 3);
						c.table_columns[k].display_data.text_alignment = aui_text_alignment(temp);
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Decimal alignment");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						const char* items[] = { "left (leading)", "right (trailing)", "none" };
						temp = int32_t(c.table_columns[k].internal_data.decimal_alignment);
						ImGui::Combo("##Decimal alignment", &temp, items, 3);
						c.table_columns[k].internal_data.decimal_alignment = aui_text_alignment(temp);
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Dynamic cell tooltip");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						ImGui::Checkbox("##Dynamic cell tooltip", &(c.table_columns[k].internal_data.has_dy_cell_tooltip));
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Cell tooltip key");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						if(!c.table_columns[k].internal_data.has_dy_cell_tooltip) {
							ImGui::InputText("##Cell tooltip key", &(c.table_columns[k].display_data.cell_tooltip_key));
						} else {
							ImGui::Text("Off");
						}
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}


					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Column is sortable");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						ImGui::Checkbox("##Column is sortable", &(c.table_columns[k].internal_data.sortable));
						ImGui::PopID();

						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Header key");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						ImGui::InputText("##Header key", &(c.table_columns[k].display_data.header_key));
						ImGui::PopID();
						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Header has background");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer || c.template_id != -1) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						ImGui::Checkbox("##Header has background", &(c.table_columns[k].internal_data.header_background));
						ImGui::PopID();
						if (is_spacer || c.template_id != -1) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Header background:");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer || c.template_id != -1) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						if(c.table_columns[k].internal_data.header_background) {
							std::string tex = (c.table_columns[k].display_data.header_texture.size() > 0 ? c.table_columns[k].display_data.header_texture : std::string("[none]"));
							ImGui::Text("%s", tex.c_str());
							ImGui::SameLine();
							if(ImGui::Button("Change row alt")) {
								auto new_file = fs::pick_existing_file(L"");
								//auto breakpt = new_file.find_last_of(L'\\');
								//c.table_columns[k].display_data.header_texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
								c.table_columns[k].display_data.header_texture = fs::native_to_utf8(relative_file_name(new_file, open_project.project_directory));
							}
						} else {
							ImGui::Text("Off");
						}
						ImGui::PopID();
						if (is_spacer || c.template_id != -1) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Header text color");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);

						if(c.template_id == -1) {
							const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
							temp = int32_t(c.table_columns[k].display_data.header_text_color);
							ImGui::Combo("##Header text color", &temp, items, 16);
							c.table_columns[k].display_data.header_text_color = text_color(temp);
						} else {
							int32_t color_choice = int32_t(c.table_columns[k].display_data.header_text_color);
							make_color_combo_box(color_choice, "##Cell text color", open_templates);
							c.table_columns[k].display_data.header_text_color = text_color(color_choice);
						}

						ImGui::PopID();
						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Dynamic header tooltip");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);

						const char* items[] = { "black", "white", "red", "green", "yellow", "unspecified", "light blue", "dark blue", "orange", "lilac", "light gray", "dark gray", "dark green", "gold", "reset", "brown" };
						temp = int32_t(c.table_columns[k].display_data.header_text_color);
						ImGui::Checkbox("##Dynamic header tooltip", &(c.table_columns[k].internal_data.has_dy_header_tooltip));
						c.table_columns[k].display_data.header_text_color = text_color(temp);

						ImGui::PopID();
						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}



					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Header tooltip key");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);
						bool is_spacer = c.table_columns[k].internal_data.cell_type == table_cell_type::spacer;
						if (is_spacer) {
							ImGui::BeginDisabled();
						}

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);

						if(!c.table_columns[k].internal_data.has_dy_header_tooltip) {
							ImGui::InputText("##Header tooltip key", &(c.table_columns[k].display_data.header_tooltip_key));
						} else {
							ImGui::Text("Off");
						}

						ImGui::PopID();
						if (is_spacer) {
							ImGui::EndDisabled();
						}
					}

					// keep it last to avoid complications
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Delete");

					for (int k = 0; k < amount_of_columns; k++) {
						ImGui::TableSetColumnIndex(k + 1);

						ImGui::PushID(k);
						ImGui::SetNextItemWidth(100.f);
						if(ImGui::Button("DEL")) {
							c.table_columns.erase(c.table_columns.begin() + k);
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}

					ImGui::EndTable();

				}

				if(ImGui::Button("Add column")) {
					c.table_columns.emplace_back();
				}
			}

			ImGui::End();
		}


		if(last_scroll_value > 0.0f) {
			if(!io.WantCaptureMouse)
				ui_scale += 1.0f;
			last_scroll_value = 0.0f;
		} else if(last_scroll_value < 0.0f) {
			if(!io.WantCaptureMouse && ui_scale > 1.0f)
				ui_scale -= 1.0f;
			last_scroll_value = 0.0f;
		}

		if(just_chose_window)
			just_chose_window = false;
		else
			chosen_window = -1;

		if(io.MouseDown[0]) {
			if(!dragging && control_drag_target == drag_target::none && !io.WantCaptureMouse) {
				selected_control = -1;

				if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];

					for(auto i = win.children.size(); i-- > 0; ) {
						auto& c = win.children[i];
						auto ct = test_rect_target(io.MousePos.x, io.MousePos.y, c.x_pos, c.y_pos, c.x_size * ui_scale, c.y_size * ui_scale, ui_scale);
						if(ct != drag_target::none) {
							selected_control = int32_t(i);
							current_edit_target = edit_targets::control;
							control_drag_target = ct;

							drag_start_x = io.MousePos.x;
							drag_start_y = io.MousePos.y;
							auto lc = find_control(win.layout, selected_control);
							if(lc) {
								base_values.x_pos = lc->abs_x;
								base_values.y_pos = lc->abs_y;
							} else {
								base_values.x_pos = c.x_pos;
								base_values.y_pos = c.y_pos;
							}

							base_values.x_size = c.x_size;
							base_values.y_size = c.y_size;
							break;
						}
					}

					if(selected_control == -1) {
						auto t = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale);
						if(t == drag_target::center) {
							drag_start_x = io.MousePos.x - drag_offset_x;
							drag_start_y = io.MousePos.y - drag_offset_y;
							dragging = true;
						} else {
							control_drag_target = t;
							drag_start_x = io.MousePos.x;
							drag_start_y = io.MousePos.y;
							base_values.x_pos = win.wrapped.x_pos;
							base_values.y_pos = win.wrapped.y_pos;
							base_values.x_size = win.wrapped.x_size;
							base_values.y_size = win.wrapped.y_size;
						}
					}
				}

				if(control_drag_target == drag_target::none) {
					drag_start_x = io.MousePos.x - drag_offset_x;
					drag_start_y = io.MousePos.y - drag_offset_y;
					dragging = true;
				}
			}
			if(dragging) {
				drag_offset_x = io.MousePos.x - drag_start_x;
				drag_offset_y = io.MousePos.y - drag_start_y;
				ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeAll);
			} else if(control_drag_target != drag_target::none) {
				auto offset_x = (io.MousePos.x - drag_start_x) / ui_scale;
				auto offset_y = (io.MousePos.y - drag_start_y) / ui_scale;
				mouse_to_drag_type(control_drag_target);

				if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];

					if(selected_control == -1) {
						switch(control_drag_target) {
							case drag_target::center:
								win.wrapped.x_pos = int16_t(win.wrapped.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								win.wrapped.y_pos = int16_t(win.wrapped.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								break;
							case drag_target::left:
							{
								auto right = win.wrapped.x_pos + win.wrapped.x_size;
								win.wrapped.x_pos = int16_t(win.wrapped.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
							}
							break;
							case drag_target::right:
							{
								auto right = int16_t(win.wrapped.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
							}
							break;
							case drag_target::top:
							{
								auto bottom = win.wrapped.y_pos + win.wrapped.y_size;
								win.wrapped.y_pos = int16_t(win.wrapped.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							case drag_target::bottom:
							{
								auto bottom = int16_t(win.wrapped.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							case drag_target::top_left:
							{
								auto right = win.wrapped.x_pos + win.wrapped.x_size;
								win.wrapped.x_pos = int16_t(win.wrapped.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
								auto bottom = win.wrapped.y_pos + win.wrapped.y_size;
								win.wrapped.y_pos = int16_t(win.wrapped.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							case drag_target::top_right:
							{
								auto right = int16_t(win.wrapped.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
								auto bottom = win.wrapped.y_pos + win.wrapped.y_size;
								win.wrapped.y_pos = int16_t(win.wrapped.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							case drag_target::bottom_left:
							{
								auto right = win.wrapped.x_pos + win.wrapped.x_size;
								win.wrapped.x_pos = int16_t(win.wrapped.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
								auto bottom = int16_t(win.wrapped.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							case drag_target::bottom_right:
							{
								auto right = int16_t(win.wrapped.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								win.wrapped.x_size = int16_t(std::max(0, right - win.wrapped.x_pos));
								auto bottom = int16_t(win.wrapped.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								win.wrapped.y_size = int16_t(std::max(0, bottom - win.wrapped.y_pos));
							}
							break;
							default: break;
						}
					} else {
						auto& c = win.children[selected_control];
						switch(control_drag_target) {
							case drag_target::center:
							{
								auto lc = find_control(win.layout, selected_control);
								if(lc && lc->absolute_position) {
									lc->abs_x = int16_t(base_values.x_pos + offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
									lc->abs_y = int16_t(base_values.y_pos + offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								}
							} break;
							case drag_target::left:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size - offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size - offset_x, float(open_project.grid_size)))));
							}
							break;
							case drag_target::right:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size + offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size + offset_x, float(open_project.grid_size)))));
							}
							break;
							case drag_target::top:
							{
								c.y_size = int16_t(std::max(0.0f, base_values.y_size - offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size - offset_y, float(open_project.grid_size)))));
							}
							break;
							case drag_target::bottom:
							{
								c.y_size = int16_t(std::max(0.0f, base_values.y_size + offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size + offset_y, float(open_project.grid_size)))));
							}
							break;
							case drag_target::top_left:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size - offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size - offset_x, float(open_project.grid_size)))));
								c.y_size = int16_t(std::max(0.0f, base_values.y_size - offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size - offset_y, float(open_project.grid_size)))));
							}
							break;
							case drag_target::top_right:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size + offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size + offset_x, float(open_project.grid_size)))));
								c.y_size = int16_t(std::max(0.0f, base_values.y_size - offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size - offset_y, float(open_project.grid_size)))));
							}
							break;
							case drag_target::bottom_left:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size - offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size - offset_x, float(open_project.grid_size)))));
								c.y_size = int16_t(std::max(0.0f, base_values.y_size + offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size + offset_y, float(open_project.grid_size)))));
							}
							break;
							case drag_target::bottom_right:
							{
								c.x_size = int16_t(std::max(0.0f, base_values.x_size + offset_x - (c.no_grid ? 0.0f : std::fmod(base_values.x_size + offset_x, float(open_project.grid_size)))));
								c.y_size = int16_t(std::max(0.0f, base_values.y_size + offset_y - (c.no_grid ? 0.0f : std::fmod(base_values.y_size + offset_y, float(open_project.grid_size)))));
							}
							break;
							default: break;
						}
					}
				}
			}
		} else {
			if(dragging) {
				dragging = false;
			} else if(control_drag_target != drag_target::none) {
				control_drag_target = drag_target::none;
			} else if(!io.WantCaptureMouse) {
				if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];
					hovered_control = -1;

					for(auto i = win.children.size(); i-- > 0; ) {
						auto& c = win.children[i];
						auto ct = test_rect_target(io.MousePos.x, io.MousePos.y, c.x_pos, c.y_pos, c.x_size * ui_scale, c.y_size * ui_scale, ui_scale);
						if(ct != drag_target::none) {
							hovered_control = int32_t(i);
							mouse_to_drag_type(ct);
							ImGui::SetTooltip("%s", c.name.c_str());
							break;
						}
					}

					if(hovered_control == -1) {
						auto t = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale);
						if(t != drag_target::none) {
							mouse_to_drag_type(t);
							ImGui::SetTooltip("%s", win.wrapped.name.c_str());
						}
					}
				}
			}
		}

		// Rendering
		ImGui::Render();

		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glUseProgram(ui_shader_program);
		glUniform1i(glGetUniformLocation(ui_shader_program, "texture_sampler"), 0);
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_width"), float(display_w));
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_height"), float(display_h));
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
			auto& win = open_project.windows[selected_window];
			bool highlightwin = selected_control == -1 && (test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale) != drag_target::none || control_drag_target != drag_target::none);
			render_window(win, (win.wrapped.x_pos + drag_offset_x / ui_scale), (win.wrapped.y_pos + drag_offset_y / ui_scale), highlightwin, ui_scale);
		}

		//
		// Draw Grid
		//
		{
			glBindVertexArray(global_square_vao);
			glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);
			glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), 0.0f, 0.0f, float(display_w), float(display_h));
			glUniform1f(glGetUniformLocation(ui_shader_program, "grid_size"), ui_scale * float(open_project.grid_size));
			glUniform2f(glGetUniformLocation(ui_shader_program, "grid_off"), std::floor(-drag_offset_x), std::floor(-drag_offset_y));
			GLuint subroutines[2] = { 4, 0 };
			glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
		}
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glDepthRange(-1.0f, 1.0f);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	CoUninitialize();
	return 0;
}
