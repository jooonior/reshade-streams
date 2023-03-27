module;

#include "stdafx.hpp"
#include "reflection.hpp"

#include <string>

export module config;

struct config_visitor
{
	const char *section;
	reshade::api::effect_runtime *runtime;

	config_visitor(const char *section, reshade::api::effect_runtime *runtime)
		: section{ section }, runtime{ runtime }
	{}
};

struct save_visitor : config_visitor
{
	using config_visitor::config_visitor;

	void operator()(auto f) { save(f.name, f.value); }

	void save(const char *key, std::string &value) {
		reshade::config_set_value(runtime, section, key, value.c_str());
	}

	template<typename U>
	void save(const char *key, U &value)
	{
		reshade::config_set_value(runtime, section, key, value);
	}
};

struct load_visitor : config_visitor
{
	using config_visitor::config_visitor;

	void operator()(auto f) { load(f.name, f.value); }

	bool load(const char *key, std::string &value) {
		size_t length = 0;
		if (!reshade::config_get_value(runtime, section, key, nullptr, &length))
			return false;

		value.resize(length);
		length += 1;  // null terminator
		return reshade::config_get_value(runtime, section, key, value.data(), &length);
	}

	template<typename U>
	void load(const char *key, U &value)
	{
		reshade::config_get_value(runtime, section, key, value);
	}
};

export class config
{
public:
	REFLECT(
		(std::string)(OutputName)(""),
		(int)(Framerate)(0),
		(float)(OverlayListWidth)(180.0f),
		(std::string)(StreamPrefix)("STREAM_")
	)

private:
	static constexpr auto SECTION = "Streams";

public:
	void load(reshade::api::effect_runtime *runtime)
	{
		reflector::for_each(*this, load_visitor(SECTION, runtime));
	}

	void save(reshade::api::effect_runtime *runtime)
	{
		reflector::for_each(*this, save_visitor(SECTION, runtime));
	}
};
