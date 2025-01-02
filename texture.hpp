#pragma once
#include <stdint.h>
#include <string>

namespace ogl {

class texture {
public:
	uint32_t texture_handle = 0;
	bool loaded = false;

	texture() { }
	texture(texture const&) noexcept;
	texture(texture&& other) noexcept;
	~texture();
	texture& operator=(texture const&) noexcept;
	texture& operator=(texture&& other) noexcept;
	void load(std::wstring const& file);
	void unload();
};

}
