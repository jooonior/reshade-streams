module;

#include "stdafx.hpp"

#include <Windows.h>

#include <format>
#include <memory>
#include <string_view>

export module winutils;

export struct win_string : std::string_view
{
	win_string(LPSTR buffer) : std::string_view(buffer), _ptr{ buffer, ::LocalFree } {}

	win_string(LPSTR buffer, std::size_t size) : std::string_view(buffer, size), _ptr{ buffer, ::LocalFree } {}

private:
	std::unique_ptr<CHAR, decltype(::LocalFree) *> _ptr;
};

template <>
struct std::formatter<win_string> : std::formatter<std::string_view> {};

export const win_string describe_error(DWORD error)
{
	LPSTR message = nullptr;

	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								 NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message, 0, NULL);

	size -= 1;  // without newline

	return { message, size };
}

export const win_string describe_last_error()
{
	return describe_error(GetLastError());
}

export void close_handle(HANDLE &handle)
{
	if (handle == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(handle);
	handle = INVALID_HANDLE_VALUE;
}

