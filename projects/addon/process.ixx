module;

#include "stdafx.hpp"

#include <Windows.h>
#include <wil/resource.h>

#include <format>
#include <stdexcept>

export module process;

import utils;
import windows;

export struct process_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

export class process
{
public:
	void redirect_input();

	void redirect_output(const char *file);

	void start(const char *path, char *args);

	void send_input(const void *data, size_t length);

	int wait_for_exit();

	void close() noexcept;

	process() = default;

	// No copying.
	process(const process &) = delete;
	process &operator=(const process &) = delete;

	process(process &&) = default;
	process &operator=(process &&) = default;

private:
	struct pipe {
		wil::unique_handle read;
		wil::unique_handle write;
	};

	pipe _stdin;
	pipe _stdout;
	// pipe _stderr;

	wil::unique_process_information _process_info;
};

SECURITY_ATTRIBUTES security_attributes = {
	.nLength = sizeof(SECURITY_ATTRIBUTES),
	.lpSecurityDescriptor = nullptr,
	.bInheritHandle = TRUE,
};

void process::redirect_input()
{
	try
	{
		HANDLE read, write;
		win::CreatePipe(&read, &write, &security_attributes, 0);

		_stdin.read.reset(read);
		_stdin.write.reset(write);

		win::SetHandleInformation(_stdin.write.get(), HANDLE_FLAG_INHERIT, 0);
	}
	catch (std::exception &)
	{
		std::throw_with_nested(process_error("Could not set up input redirection."));
	}
}

void process::redirect_output(const char *file)
{
	try
	{
		HANDLE h = win::CreateFileA(file, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
									&security_attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		_stdout.write.reset(h);
	}
	catch (std::exception &)
	{
		auto message = std::format("Could not set up output redirection to '{}'.", file);
		std::throw_with_nested(process_error(message));
	}
}

void process::start(const char *path, char *args)
{
	STARTUPINFOA startup_info = {
		.cb = sizeof(STARTUPINFO),
		.dwFlags = STARTF_USESTDHANDLES,
		.hStdInput = _stdin.read.get(),
		.hStdOutput = _stdout.write.get(),
		.hStdError = _stdout.write.get(),
	};

	try
	{
		win::CreateProcessA(path, args, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup_info, &_process_info);
		_stdin.read.reset();
		_stdout.write.reset();
	}
	catch (std::exception)
	{
		auto message = std::format("Could not start process: {} {}", path ? path : "", args ? args : "");
		std::throw_with_nested(process_error(message));
	}
}

// Blocking.
void process::send_input(const void *data, size_t length)
{
	DWORD written = 0;

	try
	{
		win::WriteFile(_stdin.write.get(), data, DWORD(length), &written, NULL);
	}
	catch (std::exception &)
	{
		std::throw_with_nested(process_error("Could not write to standard input."));
	}

	if (written != length) {
		log_warning("WriteFile : not all bytes were written");
	}
}

int process::wait_for_exit() {
	// No more input.
	_stdin.write.reset();

	try
	{
		win::WaitForSingleObject(_process_info.hProcess, INFINITE);

		DWORD exit_code;
		win::GetExitCodeProcess(_process_info.hProcess, &exit_code);

		return exit_code;
	}
	catch (std::exception &)
	{
		std::throw_with_nested(process_error("Could not wait for process to exit."));
	}
}

void process::close() noexcept
{
	_process_info.reset();
	_stdin.write.reset();
	_stdin.read.reset();
	_stdout.read.reset();
	_stdout.write.reset();
}
