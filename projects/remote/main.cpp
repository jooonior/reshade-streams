#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept>

#include <Windows.h>

import winutils;

HANDLE connect(const char *pipe_name)
{
	win::WaitNamedPipeA(pipe_name, 2000);

	HANDLE pipe = win::CreateFileA(
		pipe_name,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	DWORD mode = PIPE_READMODE_MESSAGE;
	win::SetNamedPipeHandleState(pipe, &mode, NULL, NULL);

	return pipe;
}

void send(HANDLE pipe, std::string_view message)
{
	DWORD bytes_written;
	win::WriteFile(pipe, message.data(), message.size(), &bytes_written, NULL);

	if (bytes_written != message.size())
		throw std::runtime_error("Could not send entire message.");
}

bool receive(HANDLE pipe, std::string &buffer)
{
	auto res = win::res::ReadFile(pipe, buffer.data(), 0, NULL, NULL);

	switch (res.err())
	{
	case ERROR_SUCCESS:
	case ERROR_MORE_DATA:
		// A message can be read.
		break;
	default:
		throw res.make_error();
	}

	DWORD message_size;
	win::PeekNamedPipe(pipe, NULL, 0, NULL, NULL, &message_size);

	buffer.resize(message_size);

	if (message_size == 0)
		return false;

	DWORD bytes_read;
	win::ReadFile(pipe, buffer.data(), message_size, &bytes_read, NULL);

	if (bytes_read != message_size)
		throw std::runtime_error("Could not read entire message.");

	return true;
}

void print(std::string_view message)
{
	for (std::size_t first = 0, last = 0; first < message.size(); first = last)
	{
		std::cout << "< ";

		last = message.find('\n', first);

		if (last == std::string::npos)
		{
			last = message.size();
			std::cout << message.substr(first, last - first) << '\n';
		}
		else
		{
			last++;
			std::cout << message.substr(first, last - first);
		}
	}

	std::cout << std::flush;
}

bool run(const char *pipe_name)
{
	try
	{
		std::cerr << "Connecting to '" << pipe_name << "'..." << std::endl;
		win::unique_data<HANDLE, CloseHandle, INVALID_HANDLE_VALUE> pipe = connect(pipe_name);
		std::cerr << "Connected. (use CTRL+C to exit)" << std::endl;

		std::string buffer;

		while (true)
		{
			std::cout << "> ";
			if (!std::getline(std::cin, buffer))
				break;

			send(pipe, buffer);
			if (receive(pipe, buffer))
			{
				print(buffer);
			}
		}

		return true;
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}
}

int main(int argc, char *argv[])
{
	std::string id;

	switch (argc)
	{
	case 1:
		std::cout << "Enter addon instance ID.\n> ";
		std::getline(std::cin, id);
		break;
	case 2:
		id = argv[1];
		break;
	default:
		std::cerr << "Usage: " << argv[0] << " [instance ID]" << std::endl;
		return 1;
	}

	if (id.empty())
	{
		std::cerr << "No instance ID given." << std::endl;
		return 1;
	}

	auto pipe_name = std::format("\\\\.\\pipe\\reshade-streams\\{}", id);
	return run(pipe_name.c_str()) ? 0 : 1;
}
