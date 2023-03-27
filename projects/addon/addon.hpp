#pragma once

#include "overlay.hpp"

#include <reshade.hpp>

#include <vector>

import config;
import stream;

struct __declspec(uuid("d1ffba56-33cc-45a5-a53b-8e8723cf0143")) runtime_data
{
	std::vector<stream> streams;
	overlay_data overlay;
	config config;
};
