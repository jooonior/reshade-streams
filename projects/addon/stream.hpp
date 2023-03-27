#pragma once

#include <reshade.hpp>

#include <string>

struct stream
{
	reshade::api::effect_texture_variable texture_variable;
	std::string variable_name;
	bool selected = false;

	stream(reshade::api::effect_texture_variable texture_variable)
		: texture_variable{ texture_variable }
	{}
};
