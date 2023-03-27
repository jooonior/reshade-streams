#include "stdafx.hpp"
#include "addon.hpp"
#include "overlay.hpp"

void draw_overlay(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();

	for (auto &stream : data.streams)
	{
		ImGui::Checkbox(stream.variable_name.c_str(), &stream.selected);
	}
}
