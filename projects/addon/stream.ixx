module;

#include "stdafx.hpp"

#include <string>

export module stream;

export struct stream
{
	reshade::api::effect_texture_variable texture_variable;
	std::string name;
	bool selected = false;

	stream(reshade::api::effect_texture_variable texture_variable, std::string name)
		: texture_variable{ texture_variable }, name{ std::move(name) }
	{}
};
