module;

#include "stdafx.hpp"

#include <Windows.h>

export module process;

import winutils;

export class process
{
public:
	bool redirect_input();

	bool redirect_output(const char *file);

	bool start(const char *path, char *args);

	bool send_input(const void *data, size_t length);

	int wait_for_exit();

	bool close();

	process() = default;

	// No copying.
	process(const process &) = delete;
	process &operator=(const process &) = delete;

	process(process &&) = default;
	process &operator=(process &&) = default;

private:
	struct pipe {
		HANDLE read = INVALID_HANDLE_VALUE;
		HANDLE write = INVALID_HANDLE_VALUE;
	};

	pipe _stdin;
	pipe _stdout;
	// pipe _stderr;

	PROCESS_INFORMATION _process_info;
};


SECURITY_ATTRIBUTES security_attributes = {
	.nLength = sizeof(SECURITY_ATTRIBUTES),
	.lpSecurityDescriptor = nullptr,
	.bInheritHandle = TRUE,
};

bool process::redirect_input()
{
	if (!CreatePipe(&_stdin.read, &_stdin.write, &security_attributes, 0)) {
		log_error("CreatePipe : {}", describe_last_error());
		return false;
	}

	if (!SetHandleInformation(_stdin.write, HANDLE_FLAG_INHERIT, 0)) {
		log_error("SetHandleInformation : {}", describe_last_error());
		return false;
	}

	return true;
}

bool process::redirect_output(const char *file)
{
	HANDLE handle = CreateFileA(file, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security_attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		log_error("CreateFileA : {}", describe_last_error());
		return false;
	}

	_stdout.write = handle;

	return true;
}

bool process::start(const char *path, char *args)
{
	STARTUPINFOA startup_info = {
		.cb = sizeof(STARTUPINFO),
		.dwFlags = STARTF_USESTDHANDLES,
		.hStdInput = _stdin.read,
		.hStdOutput = _stdout.write,
		.hStdError = _stdout.write,
	};

	BOOL success = CreateProcessA(
		path,
		args,
		NULL,
		NULL,
		TRUE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&startup_info,
		&_process_info);

	close_handle(_stdin.read);
	close_handle(_stdout.write);

	if (!success) {
		log_error("CreateProcessA : {}", describe_last_error());
		return false;
	}

	close_handle(_process_info.hThread);
	return true;
}

// Blocking.
bool process::send_input(const void *data, size_t length)
{
	DWORD written = 0;
	BOOL result = WriteFile(_stdin.write, data, DWORD(length), &written, NULL);

	if (!result) {
		log_error("WriteFile : {}", describe_last_error());
		return false;
	}

	if (written != length) {
		log_warning("WriteFile : not all bytes were written");
		return false;
	}

	return true;
}

int process::wait_for_exit() {
	// No more input.
	close_handle(_stdin.write);

	if (WaitForSingleObject(_process_info.hProcess, INFINITE) != 0) {
		log_error("WaitForSingleObject : {}", describe_last_error());
		return -1;
	}

	DWORD exit_code;
	if (!GetExitCodeProcess(_process_info.hProcess, &exit_code)) {
		log_error("GetExitCodeProcess : {}", describe_last_error());
		return -1;
	}

	return exit_code;
}

bool process::close()
{
	close_handle(_process_info.hProcess);
	close_handle(_stdin.write);
	close_handle(_stdout.read);

	return true;
}
