#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
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

open_project_t bytes_to_project(serialization::in_buffer& buffer);
void project_to_aui_bytes(open_project_t const& p, serialization::out_buffer& buffer);
void project_to_bytes(open_project_t const& p, serialization::out_buffer& buffer);

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
			"if(realx <= 2 || realy <= 2 || realx >= (d_rect.z -2) || realy >= (d_rect.w -2))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.25f);\n"
		"}\n"
		"vec4 grid_texture(vec2 tc) {\n"
			"float realx = grid_off.x + tc.x * d_rect.z;\n"
			"float realy = grid_off.y + tc.y * d_rect.w;\n"
			"if(mod(realx, grid_size) < 1.0f || mod(realy, grid_size) < 1.0f)\n"
				"return vec4(1.0f, 1.0f, 1.0f, 0.5f);\n"
			"return vec4(0.0f, 0.0f, 0.0f, 0.0f);\n"
		"}\n"
		"vec4 direct_texture(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2 || realy <= 2 || realx >= (d_rect.z -2) || realy >= (d_rect.w -2))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"\treturn texture(texture_sampler, tc);\n"
		"}\n"
		"vec4 frame_stretch(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2 || realy <= 2 || realx >= (d_rect.z -2) || realy >= (d_rect.w -2))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"vec2 tsize = textureSize(texture_sampler, 0);\n"
			"float xout = 0.0;\n"
			"float yout = 0.0;\n"
			"if(realx <= border_size)\n"
				"xout = realx / tsize.x;\n"
			"else if(realx >= (d_rect.z - border_size))\n"
				"xout = (1.0 - border_size / tsize.x) + (border_size - (d_rect.z - realx)) / tsize.x;\n"
			"else\n"
				"xout = border_size / tsize.x + (1.0 - 2.0 * border_size / tsize.x) * (realx - border_size) / (d_rect.z * 2.0 * border_size);\n"
			"if(realy <= border_size)\n"
				"yout = realy / tsize.y;\n"
			"else if(realy >= (d_rect.w - border_size))\n"
				"yout = (1.0 - border_size / tsize.y) + (border_size - (d_rect.w - realy)) / tsize.y;\n"
			"else\n"
				"yout = border_size / tsize.y + (1.0 - 2.0 * border_size / tsize.y) * (realy - border_size) / (d_rect.w * 2.0 * border_size);\n"
			"return texture(texture_sampler, vec2(xout, yout));\n"
		"}\n"
		"vec4 coloring_function(vec2 tc) {\n"
			"\tswitch(int(subroutines_index.x)) {\n"
				"\tcase 1: return empty_rect(tc);\n"
				"\tcase 2: return direct_texture(tc);\n"
				"\tcase 3: return frame_stretch(tc);\n"
				"\tcase 4: return grid_texture(tc);\n"
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


void render_textured_rect(color3f color, int32_t ix, int32_t iy, int32_t iwidth, int32_t iheight, GLuint texture_handle) {
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
void render_stretch_textured_rect(color3f color, int32_t ix, int32_t iy, int32_t iwidth, int32_t iheight, float border_size, GLuint texture_handle) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform1f(glGetUniformLocation(ui_shader_program, "border_size"), border_size);
	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	GLuint subroutines[2] = { 2, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_empty_rect(color3f color, int32_t ix, int32_t iy, int32_t iwidth, int32_t iheight) {
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

static void glfw_error_callback(int error, const char* description) {
	MessageBoxW(nullptr, L"GLFW Error", L"OpenGL error", MB_OK);
}

enum class drag_target {
	none, center, left, right, top, bottom, top_left, top_right, bottom_left, bottom_right
};

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
	ImGui::StyleColorsDark();

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
	if(RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Accessibility", 0, KEY_NOTIFY | KEY_QUERY_VALUE, &reg_text_key) == ERROR_SUCCESS) {
		DWORD scale = 0;
		DWORD cb = sizeof scale;
		RegQueryValueEx(reg_text_key, L"TextScaleFactor", NULL, NULL, (LPBYTE)&scale, &cb);
		if(scale != 0) {
			text_scale *= float(scale) / 100.0f;
		}
		RegCloseKey(reg_text_key);
	}
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", text_scale * 18.0f);

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

	
	//style.ScaleAllSizes(scale_value);
	CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	float ui_scale = std::max(1.0f, std::floor(text_scale + 0.5f));
	float drag_offset_x = 0.0f;
	float drag_offset_y = 0.0f;

	float drag_start_x = 0.0f;
	float drag_start_y = 0.0f;
	struct {
		int32_t x_pos;
		int32_t y_pos;
		int32_t x_size;
		int32_t y_size;
	} base_values;
	bool dragging = false;

	open_project_t open_project;
	open_project.grid_size = 10;

	int32_t selected_window = -1;
	int32_t selected_control = -1;
	drag_target control_drag_target = drag_target::none;

	int32_t display_w = 0;
	int32_t display_h = 0;
	auto switch_to_window = [&](int32_t i) { 
		if(0 <= i && i < int32_t(open_project.windows.size())) {
			selected_window = i;
			drag_offset_x = display_w / 2.0f - open_project.windows[i].wrapped.x_pos * ui_scale;
			drag_offset_y = display_h / 2.0f - open_project.windows[i].wrapped.y_pos * ui_scale - open_project.windows[i].wrapped.y_size * ui_scale / 2.0f;
		} else {
			selected_window = -1;
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

		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_FirstUseEver);

			std::string pname = "Project: " + fs::native_to_utf8(open_project.project_name);
			ImGui::Begin(pname.c_str());

			if(ImGui::Button("New")) {
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
			ImGui::SameLine();
			if(ImGui::Button("Open")) {
				auto new_file = fs::pick_existing_file(L"aui");
				if(new_file.length() > 0) {
					auto breakpt = new_file.find_last_of(L'\\');
					auto rem = new_file.substr(breakpt + 1);
					auto ext_pos = rem.find_last_of(L'.');
					fs::file loaded_file{ new_file };
					serialization::in_buffer file_content{ loaded_file.content().data, loaded_file.content().file_size };
					open_project = bytes_to_project(file_content);
					open_project.project_name = rem.substr(0, ext_pos);
					open_project.project_directory = new_file.substr(0, breakpt + 1);
					switch_to_window(0);
				}
			}


			if(open_project.project_name.empty() == false) {
				ImGui::SameLine();
				if(ImGui::Button("Save")) {
					serialization::out_buffer bytes;
					project_to_bytes(open_project, bytes);
					fs::write_file(open_project.project_directory + open_project.project_name + L".aui", bytes.data(), uint32_t(bytes.size()));
				}
				if(open_project.source_path.empty() == false) {
					ImGui::SameLine();
					if(ImGui::Button("Generate")) {
						generator::code_snippets old_code;
						{
							fs::file loaded_file{ open_project.project_directory + open_project.source_path + open_project.project_name + L".cpp" };
							old_code = generator::extract_snippets(loaded_file.content().data, loaded_file.content().file_size);
						}
						auto text = generator::generate_project_code(open_project, old_code);
						fs::write_file(open_project.project_directory + open_project.source_path + open_project.project_name + L".cpp", text.c_str(), uint32_t(text.size()));
					}
				}

				ImGui::NewLine();

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
				ImGui::InputInt("Grid size", &open_project.grid_size);
				if(open_project.grid_size < 1) {
					open_project.grid_size = 1;
				}
			}

			ImGui::End();
		}

		if(open_project.project_name.empty() == false) {
			ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

			ImGui::Begin("Containers");

			if(ImGui::Button("New")) {
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

			for(uint32_t i = 0; i < open_project.windows.size(); ++i) {
				ImGui::PushID(int32_t(i));
				if(ImGui::CollapsingHeader(open_project.windows[i].wrapped.name.c_str())) {
					if(ImGui::Button("Show")) {
						switch_to_window(int32_t(i));
					}

					ImGui::InputText("Name", &(open_project.windows[i].wrapped.temp_name));
					ImGui::SameLine();
					if(ImGui::Button("Change Name")) {
						if(open_project.windows[i].wrapped.temp_name.empty()) {
							MessageBoxW(nullptr, L"Name cannot be empty", L"Invalid Name", MB_OK);
						} else {
							bool found = false;
							for(auto& w : open_project.windows) {
								if(w.wrapped.name == open_project.windows[i].wrapped.temp_name) {
									found = true;
									break;
								}
							}
							if(found) {
								MessageBoxW(nullptr, L"Name must be unique", L"Invalid Name", MB_OK);
							} else {
								open_project.windows[i].wrapped.name = open_project.windows[i].wrapped.temp_name;
							}
						}
					}
					ImGui::InputText("Parent", &(open_project.windows[i].wrapped.parent));

					int32_t temp = open_project.windows[i].wrapped.x_pos;
					ImGui::InputInt("X position", &temp);
					open_project.windows[i].wrapped.x_pos = int16_t(temp);

					temp = open_project.windows[i].wrapped.y_pos;
					ImGui::InputInt("Y position", &temp);
					open_project.windows[i].wrapped.y_pos = int16_t(temp);

					temp = open_project.windows[i].wrapped.x_size;
					ImGui::InputInt("Width", &temp);
					open_project.windows[i].wrapped.x_size = int16_t(std::max(0, temp));

					temp = open_project.windows[i].wrapped.y_size;
					ImGui::InputInt("Height", &temp);
					open_project.windows[i].wrapped.y_size = int16_t(std::max(0, temp));

					ImVec4 ccolor{ open_project.windows[i].wrapped.rectangle_color.r, open_project.windows[i].wrapped.rectangle_color.b, open_project.windows[i].wrapped.rectangle_color.g, 1.0f };
					ImGui::ColorEdit3("Outline color", (float*)&ccolor);
					open_project.windows[i].wrapped.rectangle_color.r = ccolor.x;
					open_project.windows[i].wrapped.rectangle_color.g = ccolor.z;
					open_project.windows[i].wrapped.rectangle_color.b = ccolor.y;

					ImGui::Checkbox("Ignore grid", &(open_project.windows[i].wrapped.no_grid));

					{
						const char* items[] = { "none", "texture", "bordered texture", "legacy GFX" };
						temp = int32_t(open_project.windows[i].wrapped.background);
						ImGui::Combo("Background", &temp, items, 4);
						open_project.windows[i].wrapped.background = background_type(temp);
					}

					if(open_project.windows[i].wrapped.background == background_type::existing_gfx) {
						ImGui::InputText("Texture", &(open_project.windows[i].wrapped.texture));
					} else if(open_project.windows[i].wrapped.background != background_type::none) {
						std::string tex = "Texture: " + (open_project.windows[i].wrapped.texture.size() > 0 ? open_project.windows[i].wrapped.texture : std::string("[none]"));
						ImGui::Text(tex.c_str());
						ImGui::SameLine();
						if(ImGui::Button("Change")) {
							auto new_file = fs::pick_existing_file(L"");
							auto breakpt = new_file.find_last_of(L'\\');
							open_project.windows[i].wrapped.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
							open_project.windows[i].wrapped.ogl_texture.unload();
						}
						ImGui::Checkbox("Has alternate background", &(open_project.windows[i].wrapped.has_alternate_bg));
						if(open_project.windows[i].wrapped.has_alternate_bg) {
							std::string tex = "Alternate texture: " + (open_project.windows[i].wrapped.alternate_bg.size() > 0 ? open_project.windows[i].wrapped.alternate_bg : std::string("[none]"));
							ImGui::Text(tex.c_str());
							ImGui::SameLine();
							if(ImGui::Button("Change Alternate")) {
								auto new_file = fs::pick_existing_file(L"");
								auto breakpt = new_file.find_last_of(L'\\');
								open_project.windows[i].wrapped.alternate_bg = fs::native_to_utf8(new_file.substr(breakpt + 1));
							}
						}
					}
					if(open_project.windows[i].wrapped.background == background_type::bordered_texture) {
						temp = open_project.windows[i].wrapped.border_size;
						ImGui::InputInt("Border size", &temp);
						open_project.windows[i].wrapped.border_size = int16_t(std::max(0, temp));
					}
					ImGui::Checkbox("Receive updates while hidden", &(open_project.windows[i].wrapped.updates_while_hidden));
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
				}
				ImGui::PopID();
			}
			ImGui::End();

			if(0 <= selected_window && selected_window <= int32_t(open_project.windows.size())) {
				ImGui::Begin("Controls");
				auto& win = open_project.windows[selected_window];
				if(ImGui::Button("New")) {
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

				for(uint32_t j = 0; j < win.children.size(); ++j) {
					ImGui::PushID(int32_t(j));
					auto& c = win.children[j];

					if(ImGui::CollapsingHeader(c.name.c_str())) {
						if(ImGui::Button("Delete")) {
							win.children.erase(win.children.begin() + j);
							ImGui::PopID();
							break;
						}
						if(j > 0) {
							ImGui::SameLine();
							if(ImGui::Button("Move down")) {
								std::swap(win.children[j - 1], win.children[j]);
							}
						}
						if(j + 1 < win.children.size()) {
							ImGui::SameLine();
							if(ImGui::Button("Move up")) {
								std::swap(win.children[j + 1], win.children[j]);
							}
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
									c.name = c.temp_name;
								}
							}
						}

						int32_t temp = c.x_pos;
						ImGui::InputInt("X position", &temp);
						c.x_pos = int16_t(temp);

						temp = c.y_pos;
						ImGui::InputInt("Y position", &temp);
						c.y_pos = int16_t(temp);

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

						{
							const char* items[] = { "none", "texture", "bordered texture", "legacy GFX" };
							temp = int32_t(c.background);
							ImGui::Combo("Background", &temp, items, 4);
							c.background = background_type(temp);
						}

						if(c.background == background_type::existing_gfx) {
							ImGui::InputText("Texture", &(c.texture));
						} else if(c.background != background_type::none) {
							std::string tex = "Texture: " + (c.texture.size() > 0 ? c.texture : std::string("[none]"));
							ImGui::Text(tex.c_str());
							ImGui::SameLine();
							if(ImGui::Button("Change")) {
								auto new_file = fs::pick_existing_file(L"");
								auto breakpt = new_file.find_last_of(L'\\');
								c.texture = fs::native_to_utf8(new_file.substr(breakpt + 1));
								c.ogl_texture.unload();
							}
							ImGui::Checkbox("Has alternate background", &(c.has_alternate_bg));
							if(c.has_alternate_bg) {
								std::string tex = "Alternate texture: " + (c.alternate_bg.size() > 0 ? c.alternate_bg : std::string("[none]"));
								ImGui::Text(tex.c_str());
								ImGui::SameLine();
								if(ImGui::Button("Change Alternate")) {
									auto new_file = fs::pick_existing_file(L"");
									auto breakpt = new_file.find_last_of(L'\\');
									c.alternate_bg = fs::native_to_utf8(new_file.substr(breakpt + 1));
								}
							}
						}
						if(c.background == background_type::bordered_texture) {
							temp = c.border_size;
							ImGui::InputInt("Border size", &temp);
							c.border_size = int16_t(std::max(0, temp));
						}
						ImGui::Checkbox("Receive updates while hidden", &(c.updates_while_hidden));
						{
							const char* items[] = { "none", "list", "grid" };
							temp = int32_t(c.container_type);
							ImGui::Combo("Act as container", &temp, items, 3);
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
						} else {
							ImGui::InputText("Child window", &(c.child_window));
							ImGui::InputText("List content", &(c.list_content));
						}
						ImGui::Checkbox("Ignore rtl flip", &(c.ignore_rtl));

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
					ImGui::PopID();
				}

				ImGui::End();
			}
		}

		// Rendering
		ImGui::Render();

		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


		if(last_scroll_value > 0.0f) {
			if(!io.WantCaptureMouse)
				ui_scale += 1.0f;
			last_scroll_value = 0.0f;
		} else if(last_scroll_value < 0.0f) {
			if(!io.WantCaptureMouse && ui_scale > 1.0f)
				ui_scale -= 1.0f;
			last_scroll_value = 0.0f;
		}
		
		if(io.MouseDown[0]) {
			if(!dragging && control_drag_target == drag_target::none && !io.WantCaptureMouse) {
				selected_control = -1;

				if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];

					for(auto i = win.children.size(); i-- > 0; ) {
						auto& c = win.children[i];
						auto ct = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + c.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + c.y_pos * ui_scale + drag_offset_y, c.x_size * ui_scale, c.y_size * ui_scale, ui_scale);
						if(ct != drag_target::none) {
							selected_control = int32_t(i);
							control_drag_target = ct;

							drag_start_x = io.MousePos.x;
							drag_start_y = io.MousePos.y;
							base_values.x_pos = c.x_pos;
							base_values.y_pos = c.y_pos;
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
								auto bottom = int16_t(win.wrapped.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y- std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
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
								c.x_pos = int16_t(c.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								c.y_pos = int16_t(c.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								break;
							case drag_target::left:
							{
								auto right = c.x_pos + c.x_size;
								c.x_pos = int16_t(c.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
							}
							break;
							case drag_target::right:
							{
								auto right = int16_t(c.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
							}
							break;
							case drag_target::top:
							{
								auto bottom = c.y_pos + c.y_size;
								c.y_pos = int16_t(c.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
							}
							break;
							case drag_target::bottom:
							{
								auto bottom = int16_t(c.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
							}
							break;
							case drag_target::top_left:
							{
								auto right = c.x_pos + c.x_size;
								c.x_pos = int16_t(c.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
								auto bottom = c.y_pos + c.y_size;
								c.y_pos = int16_t(c.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
							}
							break;
							case drag_target::top_right:
							{
								auto right = int16_t(c.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
								auto bottom = c.y_pos + c.y_size;
								c.y_pos = int16_t(c.no_grid ? (base_values.y_pos + offset_y) : (base_values.y_pos + offset_y - std::fmod(base_values.y_pos + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
							}
							break;
							case drag_target::bottom_left:
							{
								auto right = c.x_pos + c.x_size;
								c.x_pos = int16_t(c.no_grid ? (base_values.x_pos + offset_x) : (base_values.x_pos + offset_x - std::fmod(base_values.x_pos + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
								auto bottom = int16_t(c.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
							}
							break;
							case drag_target::bottom_right:
							{
								auto right = int16_t(c.no_grid ? (base_values.x_pos + base_values.x_size + offset_x) : (base_values.x_pos + base_values.x_size + offset_x - std::fmod(base_values.x_pos + base_values.x_size + offset_x, float(open_project.grid_size))));
								c.x_size = int16_t(std::max(0, right - c.x_pos));
								auto bottom = int16_t(c.no_grid ? (base_values.y_pos + base_values.y_size + offset_y) : (base_values.y_pos + base_values.y_size + offset_y - std::fmod(base_values.y_pos + base_values.y_size + offset_y, float(open_project.grid_size))));
								c.y_size = int16_t(std::max(0, bottom - c.y_pos));
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
			} else {
				if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
					auto& win = open_project.windows[selected_window];
					selected_control = -1;

					for(auto i = win.children.size(); i-- > 0; ) {
						auto& c = win.children[i];
						auto ct = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + c.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + c.y_pos * ui_scale + drag_offset_y, c.x_size * ui_scale, c.y_size * ui_scale, ui_scale);
						if(ct != drag_target::none) {
							selected_control = int32_t(i);
							mouse_to_drag_type(ct);
							break;
						}
					}

					if(selected_control == -1) {
						auto t = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale);
						mouse_to_drag_type(t);
					}
				}
			}
		}

		glUseProgram(ui_shader_program);
		glUniform1i(glGetUniformLocation(ui_shader_program, "texture_sampler"), 0);
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_width"), float(display_w));
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_height"), float(display_h));
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		
		if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
			auto& win = open_project.windows[selected_window];
			bool highlightwin = selected_control == -1 && (test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale) != drag_target::none || control_drag_target != drag_target::none);
			
			if(win.wrapped.background == background_type::none || win.wrapped.background == background_type::existing_gfx || win.wrapped.texture.empty()) {
				render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
			} else if(win.wrapped.background == background_type::texture) {
				if(win.wrapped.ogl_texture.loaded == false) {
					win.wrapped.ogl_texture.load(open_project.project_directory +  fs::utf8_to_native(win.wrapped.texture));
				}
				if(win.wrapped.ogl_texture.texture_handle == 0) {
					render_empty_rect(win.wrapped.rectangle_color* (highlightwin ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + drag_offset_y), std::max(1, int32_t(win.wrapped.x_size* ui_scale)), std::max(1, int32_t(win.wrapped.y_size* ui_scale)));
				} else {
					render_textured_rect(win.wrapped.rectangle_color* (highlightwin ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + drag_offset_y), std::max(1, int32_t(win.wrapped.x_size* ui_scale)), std::max(1, int32_t(win.wrapped.y_size* ui_scale)), win.wrapped.ogl_texture.texture_handle);
				}
			} else if(win.wrapped.background == background_type::bordered_texture) {
				if(win.wrapped.ogl_texture.loaded == false) {
					win.wrapped.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(win.wrapped.texture));
				}
				if(win.wrapped.ogl_texture.texture_handle == 0) {
					render_empty_rect(win.wrapped.rectangle_color * (highlightwin ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(win.wrapped.x_size * ui_scale)), std::max(1, int32_t(win.wrapped.y_size * ui_scale)));
				} else {
					render_stretch_textured_rect(win.wrapped.rectangle_color* (highlightwin ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + drag_offset_y), std::max(1, int32_t(win.wrapped.x_size* ui_scale)), std::max(1, int32_t(win.wrapped.y_size* ui_scale)), win.wrapped.border_size * ui_scale, win.wrapped.ogl_texture.texture_handle);
				}
			}

			for(auto i = size_t(0); i < win.children.size(); ++i) {
				auto& c = win.children[i];

				auto ct = test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + c.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + c.y_pos * ui_scale + drag_offset_y, c.x_size * ui_scale, c.y_size * ui_scale, ui_scale);
				bool highlight_control = selected_control == int32_t(i);

				if(c.background == background_type::none || c.background == background_type::existing_gfx ||  c.texture.empty()) {
					render_empty_rect(c.rectangle_color * (highlight_control ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos * ui_scale + c.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos * ui_scale + c.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
				} else if(c.background == background_type::texture) {
					if(c.ogl_texture.loaded == false) {
						c.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(c.texture));
					}
					if(c.ogl_texture.texture_handle == 0) {
						render_empty_rect(c.rectangle_color* (highlight_control ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + c.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + c.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(c.x_size* ui_scale)), std::max(1, int32_t(c.y_size* ui_scale)));
					} else {
						render_textured_rect(c.rectangle_color* (highlight_control ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + c.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + c.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(c.x_size* ui_scale)), std::max(1, int32_t(c.y_size* ui_scale)), c.ogl_texture.texture_handle);
					}
				} else if(c.background == background_type::bordered_texture) {
					if(c.ogl_texture.loaded == false) {
						c.ogl_texture.load(open_project.project_directory + fs::utf8_to_native(c.texture));
					}
					if(c.ogl_texture.texture_handle == 0) {
						render_empty_rect(c.rectangle_color * (highlight_control ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos * ui_scale + c.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos * ui_scale + c.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(c.x_size * ui_scale)), std::max(1, int32_t(c.y_size * ui_scale)));
					} else {
						render_stretch_textured_rect(c.rectangle_color* (highlight_control ? 1.0f : 0.8f), int32_t(win.wrapped.x_pos* ui_scale + c.x_pos * ui_scale + drag_offset_x), int32_t(win.wrapped.y_pos* ui_scale + c.y_pos * ui_scale + drag_offset_y), std::max(1, int32_t(c.x_size* ui_scale)), std::max(1, int32_t(c.y_size* ui_scale)), c.border_size * ui_scale, c.ogl_texture.texture_handle);
					}
				}
			}
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
