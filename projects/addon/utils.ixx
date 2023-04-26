module;

#include "stdafx.hpp"

export module utils;

export void print_exception(const std::exception &e, int level = 0)
{
	log_error("{}{}", level != 0 ? "^ " : "", e.what());
	try
	{
		std::rethrow_if_nested(e);
	}
	catch (const std::exception &nested)
	{
		print_exception(nested, level + 1);
	}
}

export
template<typename F>
struct context_manager {
	F callback;

	context_manager(F callback) : callback{ callback } {}

	~context_manager() { callback(); }
};
