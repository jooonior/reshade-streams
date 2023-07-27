module;

#include "stdafx.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

export module addon;

import config;
import stream;
import pipe_server;

export struct __declspec(uuid("d1ffba56-33cc-45a5-a53b-8e8723cf0143")) runtime_data
{
	std::vector<stream> streams;
	config config;
	pipe_server pipe_server{ "\\\\.\\pipe\\ReShadeStreams", 4 };
	bool recording = false;
};

export struct command_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

std::string join_args(auto begin, auto end)
{
	if (begin == end)
		return "";

	std::string dest;

	for (auto it = begin; it != end; ++it)
	{
		if (it->empty() || it->find_first_of(" ") != std::string::npos)
		{
			dest.append("\"").append(*it).append("\"");
		}
		else
		{
			dest.append(*it);
		}

		dest.append(" ");
	}

	dest.pop_back();

	return dest;
}

void apply_streams(std::vector<stream> &streams, std::string_view name, auto f)
{
	if (name == "*")
	{
		for (auto &stream : streams)
			f(stream);

		return;
	}

	for (auto &stream : streams)
	{
		if (stream.name != name)
			continue;

		f(stream);
		return;
	}

	throw command_error(std::format("Stream '{}' not found", name));
}

bool parse_int(std::string_view str, int &out)
{
	auto first = str.data();
	auto last = first + str.size();
	return std::from_chars(first, last, out).ptr == last;
}

export void run_command(runtime_data &data, const std::vector<std::string_view> &tokens)
{
	auto &command = tokens.front();

	if (command == "framerate")
	{
		if (tokens.size() != 2)
			throw command_error("Expected: framerate <framerate>");

		if (!parse_int(tokens[1], data.config.Framerate))
			throw command_error(std::format("Cannot parse '{}' as integer", tokens[1]));
	}
	else if (command == "output.name")
	{
		if (tokens.size() != 2)
			throw command_error("Expected: output.name <file name>");

		data.config.OutputName = tokens[1];
	}
	else if (command == "output.extension")
	{
		if (tokens.size() != 2)
			throw command_error("Expected: output.extension <extension>");

		data.config.OutputExtension = tokens[1];
	}
	else if (command == "args")
	{
		data.config.FFmpegArgs = join_args(tokens.begin() + 1, tokens.end());
	}
	else if (command == "stream.selected")
	{
		if (tokens.size() != 3 || (tokens[2] != "0" && tokens[2] != "1"))
			throw command_error("Expected: stream.selected <stream name>|* 0|1");

		bool selected = tokens[2] == "1";
		apply_streams(data.streams, tokens[1], [&](stream &s) { s.selected = selected; });
	}
	else if (command == "stream.args")
	{
		if (tokens.size() < 2)
			throw command_error("Expected: stream.args <stream name>|* [<ffmpeg argument>]...");

		std::string args = join_args(tokens.begin() + 2, tokens.end());
		apply_streams(data.streams, tokens[1], [&](stream &s) { s.ffmpeg_args = args; });
	}
	else if (command == "recording")
	{
		if (tokens.size() != 2)
			throw command_error("Expected: recording start|end|toggle");

		auto &arg = tokens[1];

		if (arg == "start")
		{
			if (data.recording)
				throw command_error("Already recording");

			if (std::none_of(data.streams.begin(), data.streams.end(), [](auto &s) { return s.selected; }))
				throw command_error("No stream selected");

			data.recording = true;
		}
		else if (arg == "end")
		{
			if (!data.recording)
				throw command_error("Not recording");

			data.recording = false;
		}
		else if (arg == "toggle")
		{
			data.recording ^= true;
		}
		else
		{
			throw command_error("Expected: recording start|end|toggle");
		}
	}
	else
	{
		throw command_error(std::format("Command '{}' not found", command));
	}
}
