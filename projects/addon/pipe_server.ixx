module;

#include "stdafx.hpp"

#include <Windows.h>

#include <concepts>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

export module pipe_server;

import winutils;

class channel
{
private:
	win::unique_data<HANDLE, CloseHandle, INVALID_HANDLE_VALUE> _pipe;

	static void close_overlapped(OVERLAPPED &overlapped)
	{
		CloseHandle(overlapped.hEvent);
	}

	win::unique_data_ptr <
		OVERLAPPED,
		close_overlapped
	> _overlapped = std::make_unique<OVERLAPPED>();

	std::string _read_buffer;
	std::stringstream _write_buffer;

	enum class state {
		CONNECTING,
		WAITING,
		READING,
		WRITING,
	} _state = state::CONNECTING;

public:
	channel(const char *name, int instances)
	{
		_pipe = win::CreateNamedPipeA(
			name,                                                  // pipe name
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,             // read/write access + overlapped mode
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // message-type pipe + message-read mode + blocking mode
			instances,                                             // number of instances
			4096,                                                  // output buffer size
			4096,                                                  // input buffer size
			0,                                                     // client time-out
			NULL                                                   // default security attributes
		);

		_overlapped->hEvent = win::CreateEventA(
		   NULL,  // default security attribute
		   TRUE,  // manual-reset event
		   TRUE,  // initial state = signaled
		   NULL   // unnamed event object
		);

		if (connect())
		{
			win::SetEvent(_overlapped->hEvent);
		}
	}

	HANDLE event() const { return _overlapped->hEvent; }

	template<typename T>
	void resume(T &responder) requires std::invocable<T, std::string_view, std::stringstream &>
	{
		DWORD transferred;
		auto res = win::res::GetOverlappedResult(_pipe, _overlapped.get(), &transferred, FALSE);

		switch (res.err())
		{
		case ERROR_SUCCESS:
		case ERROR_MORE_DATA:
			break;
		case ERROR_BROKEN_PIPE:
			if (!reconnect()) return;
			_state = state::CONNECTING;
			break;
		default:
			throw res.make_error();
		}

		switch (_state)
		{
			while (true)
			{
		case state::CONNECTING:
		case state::WRITING:
			if (!wait()) return;

		case state::WAITING:
			if (!read()) return;

		case state::READING:
			_write_buffer.seekg(0);
			_write_buffer.seekp(0);
			responder(_read_buffer, _write_buffer);
			if (!write()) return;

			}
		}
	}

	bool reconnect()
	{
		win::DisconnectNamedPipe(_pipe);
		return connect();
	}

private:
	bool connect()
	{
		_state = state::CONNECTING;

		auto res = win::res::ConnectNamedPipe(_pipe, _overlapped.get());

		switch (res.err())
		{
		case ERROR_SUCCESS:
		case ERROR_PIPE_CONNECTED:  // client is already connected
			return true;
		case ERROR_IO_PENDING:
			return false;
		default:
			throw res.make_error();
		}
	}

	bool wait()
	{
		_state = state::WAITING;

		// Try reading zero bytes to see if there's any data in the pipe.
		auto res = win::res::ReadFile(_pipe, _read_buffer.data(), 0, nullptr, _overlapped.get());

		switch (res.err())
		{
		case ERROR_SUCCESS:
		case ERROR_MORE_DATA:
			// A message can be read.
			return true;
		case ERROR_IO_PENDING:
			// Waiting to receive a message.
			return false;
		case ERROR_BROKEN_PIPE:
		case ERROR_PIPE_LISTENING:
			// Client disconnected, connect new client.
			return reconnect();
		default:
			throw res.make_error();
		}
	}

	bool read()
	{
		_state = state::READING;

		DWORD message_size;
		win::PeekNamedPipe(_pipe, nullptr, 0, nullptr, nullptr, &message_size);

		_read_buffer.resize(message_size);

		DWORD bytes_read;
		auto res = win::res::ReadFile(_pipe, _read_buffer.data(), _read_buffer.size(), &bytes_read, _overlapped.get());

		switch (res.err())
		{
		case ERROR_SUCCESS:
			// Operation completed immediately.
			return true;
		case ERROR_IO_PENDING:
			return false;
		case ERROR_BROKEN_PIPE:
		case ERROR_PIPE_LISTENING:
			// Client disconnected, connect new client.
			return reconnect();
		case ERROR_MORE_DATA:  // we checked the message size beforehand, this should not happen
		default:
			throw res.make_error();
		}
	}

	bool write()
	{
		_state = state::WRITING;

		DWORD bytes_written;
		auto res = win::res::WriteFile(_pipe, _write_buffer.view().data(), _write_buffer.tellp(), &bytes_written, _overlapped.get());

		switch (res.err())
		{
		case ERROR_SUCCESS:
			// Operation completed immediately.
			return true;
		case ERROR_IO_PENDING:
			return false;
		case ERROR_BROKEN_PIPE:
		case ERROR_PIPE_LISTENING:
			// Client disconnected, connect new client.
			return reconnect();
		default:
			throw res.make_error();
		}
	}
};

export struct pipe_server_error : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

export class pipe_server {
private:
	std::vector<channel> _pipes;
	std::vector<HANDLE> _events;

public:
	pipe_server(const char *pipe_name, int instances)
	{
		for (int i = 0; i < instances; i++) {
			_pipes.emplace_back(pipe_name, instances);
			_events.push_back(_pipes.back().event());
		}
	}

	template<typename T>
	bool tick(T responder)
	{
		try
		{
			// Check for new events.
			auto wait = win::WaitForMultipleObjects(_events.size(), _events.data(), FALSE, 0);

			if (wait == WAIT_TIMEOUT) {
				// No new events.
				return false;
			}

			auto &pipe = _pipes[wait - WAIT_OBJECT_0];

			try
			{
				pipe.resume(responder);
			}
			catch (std::exception &)
			{
				pipe.reconnect();
				throw;
			}

			return true;
		}
		catch (std::exception &e)
		{
			std::throw_with_nested(pipe_server_error("Could not process incoming message."));
		}
	}
};
