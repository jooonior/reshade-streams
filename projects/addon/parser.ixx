module;

#include "stdafx.hpp"

#include <string_view>
#include <vector>

export module parser;

export class tokenizer
{
private:
	std::string_view _input;

public:
	tokenizer(std::string_view str) : _input{ str } {}

	class token_end_iterator {};

	class token_iterator
	{
	private:
		std::string_view &_input;
		std::vector<std::string_view> _tokens;

	public:
		token_iterator(std::string_view &input) : _input{ input } { operator++(); }

		const std::vector<std::string_view> &operator*() const { return _tokens; }

		token_iterator &operator++()
		{
			_tokens.clear();

			while (true)
			{
				std::size_t begin = _input.find_first_not_of(" ");

				if (begin == std::string::npos)
				{
					_input.remove_prefix(_input.size());
					break;
				}

				if (_input[begin] == ';')
				{
					_input.remove_prefix(begin + 1);
					break;
				}

				std::size_t end;

				if (_input[begin] == '"')
				{
					begin++;
					end = _input.find_first_of('"', begin);
				}
				else
				{
					end = _input.find_first_of(" \";", begin);
				}

				if (end == std::string::npos)
					end = _input.size();

				_tokens.push_back(_input.substr(begin, end - begin));

				if (end < _input.size() && _input[end] == '"')
					end++;

				_input.remove_prefix(end);
			}

			return *this;
		}

		bool operator==(const token_end_iterator &other) const { return _input.empty() && _tokens.empty(); }
	};

	token_iterator begin() { return { _input }; }

	token_end_iterator end() { return {}; }
};
