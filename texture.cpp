#include "texture.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define GL_SILENCE_DEPRECATION
#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include "glew.h"

#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_NO_STDIO 1
#define STBI_NO_LINEAR 1
#define STBI_NO_JPEG 1
//#define STBI_NO_PNG 1
//#define STBI_NO_BMP 1
#define STBI_NO_PSD 1
//#define STBI_NO_TGA 1
#define STBI_NO_GIF 1
#define STBI_NO_HDR 1
#define STBI_NO_PIC 1
#define STBI_NO_PNM 1
#define STBI_NO_THREAD_LOCALS 1

#include "stb_image.h"
#include "filesystem.hpp"

namespace ogl {

texture::~texture() {
	if(texture_handle != 0 && loaded) {
		glDeleteTextures(1, &texture_handle);
	}
}

texture::texture(texture&& other) noexcept {
	loaded = other.loaded;
	texture_handle = other.texture_handle;

	other.loaded = false;
	other.texture_handle = 0;
}
texture& texture::operator=(texture&& other) noexcept {
	loaded = other.loaded;
	texture_handle = other.texture_handle;
	other.loaded = false;
	other.texture_handle = 0;
	return *this;
}

void texture::load(std::wstring const& file_name) {
	fs::file tex{ file_name };
	auto content = tex.content();
	int32_t file_channels = 4;
	int32_t size_x = 0;
	int32_t size_y = 0;
	auto data = stbi_load_from_memory(reinterpret_cast<uint8_t const*>(content.data), int32_t(content.file_size), &(size_x), &(size_y), &file_channels, 4);
	loaded = true;
	texture_handle = 0;
	if(data) {
		glGenTextures(1, &texture_handle);
		if(texture_handle) {
			glBindTexture(GL_TEXTURE_2D, texture_handle);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, size_x, size_y);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size_x, size_y, GL_RGBA, GL_UNSIGNED_BYTE, data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindTexture(GL_TEXTURE_2D, 0);
		}
		STBI_FREE(data);
	}
	
}
void texture::unload() {
	if(texture_handle != 0 && loaded) {
		glDeleteTextures(1, &texture_handle);
	}
	loaded = false;
	texture_handle = 0;
}

}

