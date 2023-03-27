module;

#include "stdafx.hpp"

#include <vector>

export module addon;

import config;
import stream;

export struct __declspec(uuid("d1ffba56-33cc-45a5-a53b-8e8723cf0143")) runtime_data
{
	std::vector<stream> streams;
	config config;
	bool recording = false;
};
