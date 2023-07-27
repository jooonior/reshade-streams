module;

#include <wc.hpp>

#include <Windows.h>

#include <algorithm>  // copy_n
#include <concepts>
#include <functional>  // function
#include <memory>  // unique_ptr
#include <string>
#include <system_error>

export module winutils;

export namespace win
{
	template<typename T, auto Deleter, T Init = T{} >
		requires std::invocable<decltype(Deleter), T> or std::invocable<decltype(Deleter), T &>
	struct unique_data;

	template<typename T, auto Deleter, T Init>
		requires std::is_class_v<T>
	struct unique_data<T, Deleter, Init> : T
	{
		using T::T;

		unique_data(T data) { static_cast<T &>(*this) = std::move(data); }

		unique_data(const unique_data<T, Deleter, Init> &) = delete;
		unique_data<T, Deleter, Init> &operator=(const unique_data<T, Deleter, Init> &) = delete;

		unique_data(unique_data<T, Deleter, Init> &&other)
		{
			*this = std::move(other);
		};

		unique_data<T, Deleter, Init> &operator=(unique_data<T, Deleter, Init> &&other)
		{
			static_cast<T &>(*this) = static_cast<T &&>(std::move(other));
			static_cast<T &>(other) = Init;
			return *this;
		};

		~unique_data() { Deleter(*this); }
	};

	template<typename T, auto Deleter, T Init>
		requires std::is_scalar_v<T>
	struct unique_data<T, Deleter, Init>
	{
	private:
		T _data;

	public:
		unique_data() : unique_data(Init) {}

		unique_data(T data) : _data{ std::move(data) } {}

		unique_data(const unique_data<T, Deleter, Init> &) = delete;
		unique_data<T, Deleter, Init> &operator=(const unique_data<T, Deleter, Init> &) = delete;

		unique_data(unique_data<T, Deleter, Init> &&other)
		{
			*this = std::move(other);
		};

		unique_data<T, Deleter, Init> &operator=(unique_data<T, Deleter, Init> &&other)
		{
			_data = other._data;
			other._data = Init;
			return *this;
		};

		constexpr operator T() const { return _data; }

		~unique_data() { Deleter(_data); }
	};

	template<typename T, auto Deleter>
	struct unique_data_ptr : std::unique_ptr<unique_data<T, Deleter>>
	{
		using std::unique_ptr<unique_data<T, Deleter>>::unique_ptr;

		unique_data_ptr(T data) : unique_data_ptr(new unique_data<T, Deleter>(std::move(data))) {}

		unique_data_ptr(std::unique_ptr<T> &&ptr)
			: unique_data_ptr(reinterpret_cast<unique_data_ptr<T, Deleter> &&>(ptr))
		{}
	};
}

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

		constexpr check_error(auto, DWORD err) : windows_error(std::string(Message), err) {}
	};

	template<typename T, typename E>
	class result
	{
	private:
		T _value;
		DWORD _err;

	public:
		constexpr result(T &&value, bool ok)
			: _value{ std::forward<T>(value) }, _err{ ok ? ERROR_SUCCESS : GetLastError() }
		{}

		constexpr bool ok() const { return _err == 0; }

		constexpr DWORD err() const { return _err; }

		constexpr const T &value() const &{ return _value; }

		constexpr T &&value() &&{ return std::forward<T>(_value); }

		template<typename Error = E>
		constexpr Error make_error() &{ return { _value, _err }; }

		template<typename Error = E>
		constexpr Error make_error() &&{ return { std::forward<T>(_value), _err }; }

		template<typename Error = E>
		constexpr T &&unwrap() &&
		{
			if (ok())
				return std::forward<T>(_value);

			throw std::move(*this).make_error();
		}
	};

	template<auto Function, auto Predicate, typename Error>
	struct checker
	{
		using value_type = typename decltype(std::function(Function))::result_type;

		static constexpr result<value_type, Error> invoke(value_type &&value)
		{
			bool ok = Predicate(value);
			return { std::forward<value_type>(value), ok };
		}
	};
}

#define CHECKED(...) wc::check(::__VA_ARGS__).with<win::check_error<#__VA_ARGS__>>
#define EXPORT_CHECKED(Function, ...) \
	export namespace win { constexpr auto Function = wc::static_checked<::Function, __VA_ARGS__, win::check_error<#Function>>; } \
	export namespace win::res { constexpr auto Function = wc::static_wrapped<::Function, win::checker<::Function, __VA_ARGS__, win::check_error<#Function>>::invoke>; }

#define EQUAL_TO(X) ([](const auto &x) -> bool { return x == X; })
#define NOT_EQUAL_TO(X) ([](const auto &x) -> bool { return x != X; })

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
EXPORT_CHECKED(CreateEventA, NOT_EQUAL_TO(0));
EXPORT_CHECKED(CreateNamedPipeA, NOT_EQUAL_TO(INVALID_HANDLE_VALUE));
EXPORT_CHECKED(SetEvent, EQUAL_TO(TRUE));
EXPORT_CHECKED(ConnectNamedPipe, NOT_EQUAL_TO(0));
EXPORT_CHECKED(DisconnectNamedPipe, NOT_EQUAL_TO(0));
EXPORT_CHECKED(GetOverlappedResult, NOT_EQUAL_TO(0));
EXPORT_CHECKED(PeekNamedPipe, NOT_EQUAL_TO(0));
EXPORT_CHECKED(WaitForMultipleObjects, NOT_EQUAL_TO(WAIT_FAILED));
EXPORT_CHECKED(ReadFile, NOT_EQUAL_TO(FALSE));
EXPORT_CHECKED(SetNamedPipeHandleState, NOT_EQUAL_TO(FALSE));
EXPORT_CHECKED(WaitNamedPipeA, NOT_EQUAL_TO(FALSE));
