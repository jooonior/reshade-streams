module;

#include "stdafx.hpp"

#include <format>
#include <string>
#include <string_view>

export module recording;

import process;

using std::string, std::string_view;

export class recording
{
private:
	bool _is_running = false;
	process _ffmpeg;
	string _logfile;

public:
	bool start(const string_view &executable, const string_view &filename, const string_view &args, const string_view &extra_args);

	bool is_running() const { return _is_running; }

	bool push_frame(const void *data, size_t length) { return _ffmpeg.send_input(data, length); }

	bool stop();
};

bool recording::start(const string_view &executable, const string_view &filename, const string_view &args, const string_view &extra_args)
{
	assert(!is_running());

	auto cmd = std::format("\"{}\" -f rawvideo -y {} -i - {} -- \"{}\"",
						   executable, args, extra_args, filename);
	log_debug("{}", cmd);

	_logfile = string(filename) + ".log";

	if (!_ffmpeg.redirect_input() || !_ffmpeg.redirect_output(_logfile.c_str()) || !_ffmpeg.start(nullptr, cmd.data()))
	{
		return false;
	}

	_is_running = true;

	return true;
}

bool recording::stop()
{
	assert(is_running());

	_is_running = false;

	int exit_code = _ffmpeg.wait_for_exit();
	_ffmpeg.close();

	if (exit_code < 0)
		return false;

	if (exit_code > 0)
	{
		log_warning("FFmpeg exited with code {}, check '{}' for details.", exit_code, _logfile);
	}

	return true;
}

