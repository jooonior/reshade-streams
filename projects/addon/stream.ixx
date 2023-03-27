module;

#include "stdafx.hpp"

#include <string>

export module stream;

export struct stream
{
	reshade::api::effect_texture_variable texture_variable;
	std::string variable_name;
	bool selected = false;

	stream(reshade::api::effect_texture_variable texture_variable)
		: texture_variable{ texture_variable }
	{}
};
