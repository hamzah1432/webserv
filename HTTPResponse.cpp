#include "HTTPResponse.hpp"

#include <sstream>

HTTPResponse::HTTPResponse()
{
	reset();
}

std::string HTTPResponse::getReasonPhrase(int code) const
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";

		case 301: return "Moved Permanently";
		case 302: return "Found";

		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";

		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";

		default: return "Unknown";
	}
}

void HTTPResponse::setStatus(int code)
{
	_statusCode = code;
	_reasonPhrase = getReasonPhrase(code);
}

void HTTPResponse::setHeader(
	const std::string& key,
	const std::string& value)
{
	_headers[key] = value;
}

int HTTPResponse::getStatusCode() const
{
    return _statusCode;
}

std::string HTTPResponse::getHeader(
    const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it =
        _headers.find(key);

    if (it == _headers.end())
        return "";

    return it->second;
}

bool HTTPResponse::hasHeader(
    const std::string& key) const
{
    return _headers.find(key) != _headers.end();
}

const std::string& HTTPResponse::getBody() const
{
    return _body;
}

void HTTPResponse::setBody(const std::string& body)
{
	_body = body;

	std::stringstream ss;

	ss << body.size();

	_headers["Content-Length"] = ss.str();
}

void HTTPResponse::removeHeader(
	const std::string& key)
{
	_headers.erase(key);
}

void HTTPResponse::setContentType(
	const std::string& type)
{
	_headers["Content-Type"] = type;
}

void HTTPResponse::setRedirect(
	int code,
	const std::string& location)
{
	setStatus(code);

	_headers["Location"] = location;
}

void HTTPResponse::reset()
{
	_statusCode = 200;
	_reasonPhrase = "OK";

	_headers.clear();

	_headers["Server"] = "webserv";

	_body.clear();
}

std::string HTTPResponse::build() const
{
	std::stringstream response;

	response
		<< "HTTP/1.1 "
		<< _statusCode
		<< " "
		<< _reasonPhrase
		<< "\r\n";

	std::map<std::string, std::string>::const_iterator it;

	for (it = _headers.begin(); it != _headers.end(); ++it)
	{
		response
			<< it->first
			<< ": "
			<< it->second
			<< "\r\n";
	}

	response << "\r\n";

	response << _body;

	return response.str();
}
