module;

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

	template<typename F, typename Err>
	struct checked;

	template<typename R, typename ...Args, typename Err>
	struct checked<R(__stdcall)(Args...), Err>
	{
	public:
		using return_type = R;
		using function_type = R(__stdcall)(Args...);
		using predicate_type = bool(R);
		using error_type = Err;

	private:
		function_type *function;
		predicate_type *predicate;

	public:
		consteval checked(function_type function, predicate_type predicate) : function{ function }, predicate{ predicate } {}

		constexpr return_type operator()(Args... arg) const {
			return_type result = function(std::forward<Args>(arg)...);

			if (!predicate(result))
				throw error_type(result);

			return result;
		}
	};

	template<typename F, F Function, typename Err>
	struct checked_function : checked<F, Err>
	{
	private:
		using base = checked<F, Err>;

	public:
		consteval checked_function(base::predicate_type predicate) : base(Function, predicate) {}
	};
}

#define CHECKED(...) win::checked_function<decltype(::__VA_ARGS__), ::__VA_ARGS__, win::check_error<#__VA_ARGS__>>
#define EXPORT_CHECKED(Function, ...) export namespace win { constexpr CHECKED(Function) Function(__VA_ARGS__); }

#define EQUAL_TO(X) ([](auto x) { return x == X; })
#define NOT_EQUAL_TO(X) ([](auto x) { return x != X; })

namespace win
{
	export BOOL CloseHandle(::HANDLE &object)
	{
		if (object == INVALID_HANDLE_VALUE)
			return FALSE;

		constexpr CHECKED(CloseHandle) checked_close_handle(EQUAL_TO(TRUE));
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
