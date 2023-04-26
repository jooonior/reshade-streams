module;

#include "stdafx.hpp"

#include <format>
#include <string>

export module stream;

import config;
import recording;
import utils;

export struct stream_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

export class stream
{
public:
	reshade::api::effect_texture_variable texture_variable;
	std::string name;
	bool selected = false;

private:
	recording _recording;
	reshade::api::resource _intermediate;
	std::string _filename;

public:
	stream(reshade::api::effect_texture_variable texture_variable, std::string name)
		: texture_variable{ texture_variable }, name{ std::move(name) }
	{}

	bool is_recording() const { return _recording.is_running(); }

	bool update(reshade::api::effect_runtime *runtime, bool should_record, const config &config);

private:
	void start_recording(reshade::api::effect_runtime *runtime, const config &config);

	void record_frame(reshade::api::effect_runtime *runtime);

	void end_recording();

	reshade::api::resource get_resource(reshade::api::effect_runtime *runtime)
	{
		reshade::api::device *device = runtime->get_device();

		reshade::api::resource_view view = {};
		runtime->get_texture_binding(texture_variable, &view);
		if (view == 0)
		{
			throw stream_error("Could not get stream texture binding.");
		}

		reshade::api::resource res = device->get_resource_from_view(view);
		if (res == 0)
		{
			throw stream_error("Could not get stream texture resource.");
		}

		return res;
	}
};

bool stream::update(reshade::api::effect_runtime *runtime, bool should_record, const config &config)
{
	try
	{
		if (selected && should_record != is_recording())
		{
			if (should_record)
			{
				start_recording(runtime, config);
			}
			else
			{
				end_recording();
			}
		}
	}
	catch (stream_error &e)
	{
		print_exception(e);
		return false;
	}

	if (!is_recording())
		return false;

	try
	{
		record_frame(runtime);
	}
	catch (stream_error &e)
	{
		print_exception(e);

		// This happens when FFmpeg exits because of invalid input. In that case
		// the above message doesn't say anything useful, but end_recording() below
		// fails with more useful error (process exited with nonzero code).

		try {
			end_recording();
		}
		catch (stream_error &e) {
			print_exception(e);
		}

		return false;
	}

	return true;
}

// Maps to pixel format strings recognized by ffmpeg CLI, also used from addon overlay.
export const char *convert_pixel_format(reshade::api::format fmt)
{
	switch (reshade::api::format_to_typeless(fmt))
	{
	case reshade::api::format::r8g8b8a8_typeless:
		return "rgba";
	case reshade::api::format::b8g8r8a8_typeless:
		return "bgra";
	default:
		return nullptr;
	}
}

void stream::start_recording(reshade::api::effect_runtime *runtime, const config &config)
{
	// Used in potential error messages, so set early.
	_filename = config.OutputName + name + '.' + config.OutputExtension;
	log_info("Recording '{}' to '{}'.", name, _filename);

	reshade::api::device *device = runtime->get_device();

	try
	{
		reshade::api::resource res = get_resource(runtime);
		reshade::api::resource_desc desc = device->get_resource_desc(get_resource(runtime));

		const char *pixel_format = convert_pixel_format(desc.texture.format);
		if (pixel_format == nullptr)
		{
			throw stream_error("Stream texture has an unsupported pixel format.");
		}

		reshade::api::resource_desc host_desc = {
			desc.texture.width, desc.texture.height,
			1, 1,
			format_to_default_typed(desc.texture.format),
			1,
			reshade::api::memory_heap::gpu_to_cpu, reshade::api::resource_usage::copy_dest
		};

		if (!device->create_resource(host_desc, nullptr, reshade::api::resource_usage::copy_dest, &_intermediate))
		{
			throw stream_error("Failed to create host resource.");
		}

		auto input_options = std::format("-r {} -pixel_format {} -video_size {}x{}",
										 config.Framerate, pixel_format, desc.texture.width, desc.texture.height);

		_recording.start(config.FFmpegPath, _filename, input_options, config.FFmpegArgs);
	}
	catch (std::exception &)
	{
		device->destroy_resource(_intermediate);

		auto message = std::format("Could not start recording stream '{}'.", name);
		std::throw_with_nested(stream_error(message));
	}
}

void stream::record_frame(reshade::api::effect_runtime *runtime)
{
	reshade::api::device *device = runtime->get_device();

	try
	{
		reshade::api::resource res = get_resource(runtime);
		reshade::api::resource_desc desc = device->get_resource_desc(res);

		// Copy stream texture into intermediate buffer.

		reshade::api::command_list *const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
		cmd_list->barrier(res, reshade::api::resource_usage::shader_resource, reshade::api::resource_usage::copy_source);
		cmd_list->copy_texture_region(res, 0, nullptr, _intermediate, 0, nullptr);
		cmd_list->barrier(res, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::shader_resource);

		runtime->get_command_queue()->flush_immediate_command_list();
		runtime->get_command_queue()->wait_idle();

		// Map intermediate buffer into CPU address space.

		reshade::api::subresource_data host_data;
		if (!device->map_texture_region(_intermediate, 0, nullptr, reshade::api::map_access::read_only, &host_data))
		{
			throw stream_error("Could not access stream texture data.");
		}

		context_manager unmap_texture_region([&] { device->unmap_texture_region(_intermediate, 0); });

		// Send intermediate buffer contents to recording.
		// Assumes resource properties match video parameters and fails horribly if not.

		size_t frame_size = host_data.row_pitch * desc.texture.height;
		_recording.push_frame(host_data.data, frame_size);
	}
	catch (std::exception &)
	{
		auto message = std::format("Could not record frame. Recording of stream '{}' failed.", name);
		std::throw_with_nested(stream_error(message));
	}
}

void stream::end_recording()
{
	try
	{
		_recording.stop();
		log_info("Stopped recording '{}' to '{}'.", name, _filename);
	}
	catch (std::exception &)
	{
		auto message = std::format("Could not stop recording '{}' properly, output '{}' may be corrupted.", name, _filename);
		std::throw_with_nested(stream_error(message));
	}
}
