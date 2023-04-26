module;

#include "stdafx.hpp"

#include <format>
#include <string>
#include <string_view>

export module recording;

import process;

using std::string, std::string_view;

export struct recording_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

export class recording
{
private:
	bool _is_running = false;
	process _ffmpeg;
	string _logfile;

public:
	void start(const string_view &executable, const string_view &filename, const string_view &args, const string_view &extra_args);

	bool is_running() const { return _is_running; }

	void push_frame(const void *data, size_t length)
	{
		_ffmpeg.send_input(data, length);
	}

	void stop();
};

void recording::start(const string_view &executable, const string_view &filename, const string_view &args, const string_view &extra_args)
{
	assert(!is_running());

	try
	{
		auto cmd = std::format("\"{}\" -f rawvideo -y {} -i - {} -- \"{}\"",
							   executable, args, extra_args, filename);
		log_debug("{}", cmd);

		_logfile = string(filename) + ".log";

		_ffmpeg.redirect_input();
		_ffmpeg.redirect_output(_logfile.c_str());
		_ffmpeg.start(nullptr, cmd.data());
	}
	catch (...)
	{
		_ffmpeg.close();
		throw;
	}

	_is_running = true;
}

void recording::stop()
{
	if (!is_running())
		return;

	_is_running = false;

	int exit_code;

	try
	{
		exit_code = _ffmpeg.wait_for_exit();
	}
	catch (...)
	{
		_ffmpeg.close();
		throw;
	}

	_ffmpeg.close();

	if (exit_code != 0) {
		auto message = std::format("FFmpeg exited with code {}, check '{}' for details.", exit_code, _logfile);
		throw recording_error(message);
	}
}

