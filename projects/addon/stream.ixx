module;

#include "stdafx.hpp"

#include <format>
#include <string>

export module stream;

import config;
import recording;

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

	bool start_recording(reshade::api::effect_runtime *runtime, const config &config);

	bool record_frame(reshade::api::effect_runtime *runtime);

	bool end_recording();
};

bool stream::update(reshade::api::effect_runtime *runtime, bool should_record, const config &config)
{
	if (selected && should_record != is_recording())
	{
		if (should_record)
		{
			if (!start_recording(runtime, config))
			{
				log_error("Could not start recording stream '{}'.", name);
			}
			else
			{
				log_info("Recording stream '{}' to '{}'.", name, _filename);
			}
		}
		else
		{
			if (end_recording())
			{
				log_info("Stopped recording stream '{}' to '{}'.", name, _filename);
			}
		}
	}

	if (!is_recording())
		return false;

	if (!record_frame(runtime))
	{
		log_error("Could not record frame. Recording of stream '{}' failed.", name);

		end_recording();
		return false;
	}

	return true;
}

const char *convert_pixel_format(reshade::api::format fmt)
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

bool stream::start_recording(reshade::api::effect_runtime *runtime, const config &config)
{
	// Must be set for error messages.
	_filename = config.OutputName + name + '.' + config.OutputExtension;

	reshade::api::device *device = runtime->get_device();

	reshade::api::resource_view view = {};
	runtime->get_texture_binding(texture_variable, &view);

	if (view == 0)
	{
		log_error("Could not get stream texture binding.");
		return false;
	}

	reshade::api::resource res = device->get_resource_from_view(view);
	reshade::api::resource_desc desc = device->get_resource_desc(res);

	const char *pixel_format = convert_pixel_format(desc.texture.format);
	if (!pixel_format)
	{
		log_error("Stream texture has an unsupported pixel format.");
		return false;
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
		log_error("Failed to create host resource.");
		return false;
	}

	auto args = std::format("-r {} -pixel_format {} -video_size {}x{}",
							config.Framerate, pixel_format, desc.texture.width, desc.texture.height);

	if (!_recording.start(config.FFmpegPath, _filename, args, config.FFmpegArgs))
	{
		device->destroy_resource(_intermediate);

		return false;
	}

	return true;
}

bool stream::record_frame(reshade::api::effect_runtime *runtime)
{
	reshade::api::device *device = runtime->get_device();

	reshade::api::resource_view view = {};
	runtime->get_texture_binding(texture_variable, &view);

	if (view == 0)
	{
		log_error("Could not get stream texture binding.", name);
		return false;
	}

	reshade::api::resource res = device->get_resource_from_view(view);
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
		log_error("Could not access stream texture data.");
		return false;
	}

	// Send intermediate buffer contents to recording.
	// Assumes resource properties match video parameters and fails horribly if not.

	size_t frame_size = host_data.row_pitch * desc.texture.height;
	bool success = _recording.push_frame(host_data.data, frame_size);

	device->unmap_texture_region(_intermediate, 0);

	return success;
}

bool stream::end_recording()
{
	if (!_recording.stop())
	{
		log_warning("'{}' may be corrupted.", _filename);

		return false;
	}

	return true;
}
