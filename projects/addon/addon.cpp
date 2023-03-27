#include "stdafx.hpp"
#include "addon.hpp"
#include "overlay.hpp"

#include <string>

static void on_init_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->create_private_data<runtime_data>();
}

static void on_destroy_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime->destroy_private_data<runtime_data>();
}

static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();

	// Previous data is invalidated by reload.
	data.streams.clear();

	runtime->enumerate_texture_variables(nullptr, [&](reshade::api::effect_runtime *runtime, reshade::api::effect_texture_variable variable) {
		data.streams.emplace_back(variable);
		stream &stream = data.streams.back();

		// Get buffer length required to hold the name.
		size_t length;
		runtime->get_texture_variable_name(variable, nullptr, &length);

		// Get the name itself.
		stream.variable_name.resize(length);
		length += 1;  // null terminator
		runtime->get_texture_variable_name(variable, stream.variable_name.data(), &length);
	});

	for (auto &stream : data.streams)
	{
		log_message(reshade::log_level::debug, "Found texture variable: {}", stream.variable_name);
	}
}

extern "C" __declspec(dllexport) const char *NAME = "Streams";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "High-performance video capture of game buffers and custom effect streams.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(on_reshade_reloaded_effects);
		reshade::register_overlay("Streams", draw_overlay);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
