#pragma once

#include "overlay.hpp"
#include "stream.hpp"

#include <reshade.hpp>

#include <vector>

struct __declspec(uuid("d1ffba56-33cc-45a5-a53b-8e8723cf0143")) runtime_data
{
	std::vector<stream> streams;
	overlay_data overlay;
	std::string output_name;
	int framerate = 0;
};
