#include "stdafx.hpp"

#include <sstream>
#include <string>

import addon;
import config;
import overlay;
import parser;
import utils;

static void on_init_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->create_private_data<runtime_data>();
	data.config.load(runtime);
}

static void on_destroy_effect_runtime(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();
	data.config.save(runtime);

	runtime->destroy_private_data<runtime_data>();
}

static void on_reshade_reloaded_effects(reshade::api::effect_runtime *runtime)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();

	// Previous data is invalidated by reload.
	data.streams.clear();

	runtime->enumerate_texture_variables(nullptr, [&](reshade::api::effect_runtime *runtime, reshade::api::effect_texture_variable variable) {
		// Get buffer length required to hold the name.
		size_t length;
		runtime->get_texture_variable_name(variable, nullptr, &length);

		// Get the name itself.
		std::string name(length - 1, 0);
		runtime->get_texture_variable_name(variable, name.data(), &length);

		// Only interested in variables matching our prefix.
		if (!name.starts_with(data.config.StreamPrefix))
			return;

		// Strip prefix.
		name.erase(0, data.config.StreamPrefix.size());

		data.streams.emplace_back(variable, std::move(name));
	});

	for (auto &stream : data.streams)
	{
		log_debug("Found texture variable: {}", stream.name);
	}
}

static void on_reshade_finish_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view rtv, reshade::api::resource_view)
{
	runtime_data &data = runtime->get_private_data<runtime_data>();
	reshade::api::device *const device = runtime->get_device();

	try
	{
		data.pipe_server.tick([&](std::string_view message, std::ostream &reply) {
			log_debug("Received message: {}", message);

			for (auto &tokens : tokenizer(message))
			{
				try
				{
					run_command(data, tokens);
				}
				catch (std::exception &e)
				{
					reply << e.what() << std::endl;
				}
			}
		});
	}
	catch (std::exception &e)
	{
		print_exception(e);
	}

	bool recording_streams = false;

	for (auto &stream : data.streams)
	{
		if (!stream.update(runtime, data.recording, data.config))
			continue;

		recording_streams = true;
	}

	if (data.recording && !recording_streams)
	{
		data.recording = false;
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
		reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
		reshade::register_overlay("Streams", draw_overlay);
		reshade::register_overlay(nullptr, draw_settings_overlay);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
