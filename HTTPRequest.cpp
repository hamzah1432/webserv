#include "HTTPRequest.hpp"

#include <sstream>
#include <cstdlib>

HTTPRequest::HTTPRequest()
{
	reset();
}

void HTTPRequest::reset()
{
	_valid = true;
	_errorCode = 0;

	_buffer.clear();

	_headersParsed = false;
	_complete = false;

	_contentLength = 0;

	_method.clear();
	_uri.clear();
	_version.clear();

	_headers.clear();
	_body.clear();
}

void HTTPRequest::parseRequestLine(const std::string& line)
{
	std::stringstream ss(line);

	ss >> _method;
	ss >> _uri;
	ss >> _version;
}

void HTTPRequest::parseHeadersBlock(const std::string& headersBlock)
{
	std::stringstream ss(headersBlock);

	std::string line;

	if (!std::getline(ss, line))
		return;

	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);

	parseRequestLine(line);

	while (std::getline(ss, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (line.empty())
			continue;

		size_t colon = line.find(':');

		if (colon == std::string::npos)
			continue;

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		while (!value.empty() && value[0] == ' ')
			value.erase(0, 1);

		_headers[key] = value;
	}

	if (hasHeader("Content-Length") && validateContentLength())
	{
		std::stringstream ss(
			getHeader("Content-Length"));

		ss >> _contentLength;
	}
}

bool HTTPRequest::validateMethod()
{
	if (_method == "GET")
		return true;

	if (_method == "POST")
		return true;

	if (_method == "DELETE")
		return true;

	_valid = false;
	_errorCode = 501;

	return false;
}

bool HTTPRequest::validateVersion()
{
	if (_version == "HTTP/1.1")
		return true;

	if (_version == "HTTP/1.0")
		return true;

	_valid = false;
	_errorCode = 505;

	return false;
}

bool HTTPRequest::validateRequestLine()
{
	if (_method.empty())
	{
		_valid = false;
		_errorCode = 400;
		return false;
	}

	if (_uri.empty())
	{
		_valid = false;
		_errorCode = 400;
		return false;
	}

	if (_version.empty())
	{
		_valid = false;
		_errorCode = 400;
		return false;
	}

	if (!validateMethod())
		return false;

	if (!validateVersion())
		return false;

	return true;
}

//helper for validateContentLength

static bool isDigits(const std::string& str)
{
	size_t i = 0;

	if (str.empty())
		return false;

	while (i < str.size())
	{
		if (str[i] < '0' || str[i] > '9')
			return false;

		++i;
	}

	return true;
}

bool HTTPRequest::validateContentLength()
{
	if (!hasHeader("Content-Length"))
		return true;

	std::string value = getHeader("Content-Length");

	if (!isDigits(value))
	{
		_valid = false;
		_errorCode = 400;
		return false;
	}

	return true;
}

void HTTPRequest::feed(const std::string& data)
{
	if (_complete)
		return;

	_buffer += data;

	size_t headersEnd = _buffer.find("\r\n\r\n");

	if (headersEnd == std::string::npos)
		return;

	if (!_headersParsed)
	{
		parseHeadersBlock(_buffer.substr(0, headersEnd));

		_headersParsed = true;
	}

	if (!validateRequestLine())
	{
		_complete = true;
		return;
	}

	if (!validateContentLength())
	{
		_complete = true;
		return;
	}

	size_t bodyStart = headersEnd + 4;

	size_t bodySize = _buffer.size() - bodyStart;

	if (bodySize < _contentLength)
		return;

	_body = _buffer.substr(bodyStart, _contentLength);

	_complete = true;
}

bool HTTPRequest::isComplete() const
{
	return (_complete);
}

bool HTTPRequest::isValid() const
{
	return _valid;
}

int HTTPRequest::getErrorCode() const
{
	return _errorCode;
}

bool HTTPRequest::hasHeader(const std::string& key) const
{
	return (_headers.find(key) != _headers.end());
}

const std::string& HTTPRequest::getMethod() const
{
	return (_method);
}

const std::string& HTTPRequest::getURI() const
{
	return (_uri);
}

const std::string& HTTPRequest::getVersion() const
{
	return (_version);
}

const std::string& HTTPRequest::getBody() const
{
	return (_body);
}

const std::map<std::string, std::string>& HTTPRequest::getHeaders() const
{
	return (_headers);
}

std::string HTTPRequest::getHeader(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it;

	it = _headers.find(key);

	if (it == _headers.end())
		return "";

	return it->second;
}

std::string HTTPRequest::getPath() const
{
	size_t queryPos = _uri.find('?');

	if (queryPos == std::string::npos)
		return _uri;

	return _uri.substr(0, queryPos);
}

std::string HTTPRequest::getQueryString() const
{
	size_t queryPos = _uri.find('?');

	if (queryPos == std::string::npos)
		return "";

	return _uri.substr(queryPos + 1);
}

bool HTTPRequest::shouldKeepAlive() const
{
	if (!hasHeader("Connection"))
		return true;

	if (getHeader("Connection") == "close")
		return false;

	return true;
}
