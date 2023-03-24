#pragma once

#include <reshade.hpp>

struct overlay_data
{
	float stream_list_width = 180.0f;
};

void draw_overlay(reshade::api::effect_runtime *);
