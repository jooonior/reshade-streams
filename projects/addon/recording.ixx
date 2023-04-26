module;

#include "stdafx.hpp"

#include <format>
#include <string>
#include <string_view>

export module recording;

import process;

using std::string_view;

export struct recording_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

export class recording
{
private:
	bool _is_running = false;
	process _ffmpeg;
	std::string _logfile;

public:
	void start(string_view executable, string_view filename, string_view input_options, string_view output_options);

	bool is_running() const { return _is_running; }

	void push_frame(const void *data, size_t length)
	{
		_ffmpeg.send_input(data, length);
	}

	void stop();
};

void recording::start(string_view executable, string_view filename, string_view input_options, string_view output_options)
{
	assert(!is_running());

	try
	{
		auto cmd = std::format("\"{}\" -f rawvideo -y {} -i - {} -- \"{}\"",
							   executable, input_options, output_options, filename);
		log_debug("{}", cmd);

		_logfile = std::string(filename) + ".log";

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

