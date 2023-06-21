module;

#include <wc.hpp>

#include <Windows.h>

#include <algorithm>  // copy_n
#include <string>
#include <system_error>

export module winutils;

namespace win
{
	export struct windows_error : std::system_error
	{
		windows_error(const std::string &message, DWORD err)
			: std::system_error(std::error_code(err, std::system_category()), message)
		{}

		windows_error(const std::string &message) : windows_error(message, GetLastError()) {};
	};

	template<std::size_t size>
	struct string_literal {
		char chars[size];

		constexpr string_literal(const char(&str)[size]) {
			std::copy_n(str, size, chars);
		}

		constexpr operator const char *() const { return chars; }
	};

	template<string_literal Message>
	struct check_error : windows_error {
		constexpr check_error(auto) : windows_error(std::string(Message)) {}

	};
}

#define CHECKED(...) wc::check(::__VA_ARGS__).with<win::check_error<#__VA_ARGS__>>
#define EXPORT_CHECKED(Function, ...) export namespace win { constexpr auto Function = wc::static_checked<::Function, __VA_ARGS__, win::check_error<#Function>>; }

#define EQUAL_TO(X) ([](auto x) { return x == X; })
#define NOT_EQUAL_TO(X) ([](auto x) { return x != X; })

namespace win
{
	export BOOL CloseHandle(::HANDLE &object)
	{
		if (object == INVALID_HANDLE_VALUE)
			return FALSE;

		constexpr auto checked_close_handle = CHECKED(CloseHandle)(EQUAL_TO(TRUE));
		checked_close_handle(object);

		object = INVALID_HANDLE_VALUE;
		return TRUE;
	}
}

EXPORT_CHECKED(CreatePipe, EQUAL_TO(TRUE));
EXPORT_CHECKED(SetHandleInformation, EQUAL_TO(TRUE));
EXPORT_CHECKED(CreateFileA, NOT_EQUAL_TO(INVALID_HANDLE_VALUE));
EXPORT_CHECKED(CreateProcessA, EQUAL_TO(TRUE));
EXPORT_CHECKED(WriteFile, EQUAL_TO(TRUE));
EXPORT_CHECKED(WaitForSingleObject, NOT_EQUAL_TO(WAIT_FAILED));
EXPORT_CHECKED(GetExitCodeProcess, EQUAL_TO(TRUE));
