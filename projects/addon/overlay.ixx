module;

#include "stdafx.hpp"
#include "addon.hpp"

#include <imgui_stdlib.h>

#include <algorithm>

export module overlay;

import config;
import stream;

template<typename... Args>
void tooltip(const char *fmt, Args... args)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip(fmt, args...);
	}
}

export void draw_settings_overlay(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();

	ImGui::InputText("Stream Prefix", &data.config.StreamPrefix);
	tooltip("Only variables named with this prefix will be listed as streams. Change requires reloading effects.");
}

export void draw_overlay(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();
	reshade::api::device *device = runtime->get_device();

	ImGui::Button("Start Recording", { ImGui::GetContentRegionAvail().x, 0 });

	ImGui::PushItemWidth(std::max(ImGui::GetContentRegionAvail().x - 250.f, 180.0f));
	ImGui::InputText("Output Name", &data.config.OutputName);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::PushItemWidth(70.0f);
	ImGui::DragInt("Framerate", &data.config.Framerate, 1.0f, 0, std::numeric_limits<int>::max());
	ImGui::PopItemWidth();

	ImVec2 space_to_fill = ImGui::GetContentRegionAvail();

	ImGui::BeginChild("Stream List", { data.config.OverlayListWidth, 0.0f }, true);

	for (auto &stream : data.streams)
	{
		ImGui::Checkbox(stream.name.c_str(), &stream.selected);
	}

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::Button("Stream Panel Resize Handle", { 5.0f, space_to_fill.y });
	if (ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (ImGui::IsItemActive()) {
		// Handle is being dragged.
		const float min_width = 10.0f;
		const float max_width = space_to_fill.x - 30.0f;
		data.config.OverlayListWidth = std::clamp(data.config.OverlayListWidth + ImGui::GetIO().MouseDelta.x, min_width, max_width);
	}

	ImGui::SameLine();

	ImGui::BeginChild("Stream Details", {}, true);

	for (auto &stream : data.streams)
	{
		if (!stream.selected)
			continue;

		if (ImGui::CollapsingHeader(stream.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			reshade::api::resource_view view = {};
			runtime->get_texture_binding(stream.texture_variable, &view);

			if (view == 0)
			{
				ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Not available");
				continue;
			}

			reshade::api::resource res = device->get_resource_from_view(view);
			reshade::api::resource_desc desc = device->get_resource_desc(res);

			const float aspect_ratio = float(desc.texture.width) / float(desc.texture.height);
			float width = ImGui::GetContentRegionAvail().x;
			ImGui::Image(view.handle, { width, width / aspect_ratio });
		}
	}

	ImGui::EndChild();
}
