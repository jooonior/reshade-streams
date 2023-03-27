#pragma once

#define NOMINMAX

#define ImTextureID ImU64  // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle.
#include <imgui.h>
#include <reshade.hpp>

#include <format>
#include <string>

/// <summary>
/// <see cref="std::format"/> wrapper around <see cref="reshade::log_message"/> for logging formatted messages.
/// </summary>
template<class... Args>
inline void log_message(reshade::log_level level, std::format_string<Args...> fmt, Args&&... args)
{
	std::string message = std::format(fmt, std::forward<Args>(args)...);
	reshade::log_message(level, message.c_str());
}

#define LOG_MESSAGE_WITH_LEVEL(Level) \
	template<class... Args> \
	inline void log_##Level(std::format_string<Args...> fmt, Args&&... args) \
	{ \
		log_message(reshade::log_level::Level, fmt, std::forward<Args>(args)...); \
	}

LOG_MESSAGE_WITH_LEVEL(error);
LOG_MESSAGE_WITH_LEVEL(warning);
LOG_MESSAGE_WITH_LEVEL(info);
LOG_MESSAGE_WITH_LEVEL(debug);

#undef LOG_MESSAGE_WITH_LEVEL
