#include <lsp/MessageParser.h>
#include <json/json.h>
#include <libsolutil/JSON.h>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>

using namespace std;

namespace lsp {

variant<Json::Value, ErrorCode> parseMessage(istream& _input, ostream* _logger)
{
	// Parses a single text line ending with CRLF (or just LF).
	auto const readLine = [&]() -> string {
		string line;
		getline(_input, line);
		if (!line.empty() && line.back() == '\r')
			line.resize(line.size() - 1);
		return line;
	};

	// Reads given number of bytes from input stream.
	auto const readBytes = [&](size_t _n) {
		string data;
		data.resize(_n);
		_input.read(data.data(), _n);
		return data;
	};

	using HeaderMap = unordered_map<string, string>;

	// Parses header section including message-delimiting empty line.
	auto const parseHeaders = [&]() -> variant<HeaderMap, ErrorCode> {
		unordered_map<string, string> headers;
		for (string line = readLine(); !line.empty(); line = readLine())
		{
			auto const delimiterPos = line.find(':');
			if (delimiterPos == string::npos)
				return {ErrorCode::TransportProtocolError};

			auto const name = boost::to_lower_copy(line.substr(0, delimiterPos));
			auto const value = boost::trim_copy(line.substr(delimiterPos + 1));
			headers[name] = value;
		}
		return {headers};
	};

	auto const headers = parseHeaders();
	if (holds_alternative<ErrorCode>(headers))
		return get<ErrorCode>(headers);

	if (!get<HeaderMap>(headers).count("content-length"))
		return {ErrorCode::TransportProtocolError};

	size_t const contentLength = stoi(get<HeaderMap>(headers).at("content-length"));
	string const data = readBytes(contentLength);

	Json::Value jsonMessage;
	string errs;
	solidity::util::jsonParseStrict(data, jsonMessage, &errs);
	if (!errs.empty())
	{
		if (_logger)
			*_logger << errs << endl;
		return {ErrorCode::JsonParseError};
	}

	if (_logger)
	{
		auto const prettyPrinted = solidity::util::jsonPrettyPrint(jsonMessage);
		*_logger << "parsed message:\n" << prettyPrinted << endl;
	}

	if (jsonMessage["id"].isInt() && jsonMessage["method"].isString())
	{
	}

	return {jsonMessage};
}

} // namespace lsp

