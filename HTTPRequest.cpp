/*temp*/
#include "HTTPRequest.hpp"
#include <vector>
#include <cctype>

// --- file-local helpers ----------------------------------------------------

namespace
{
	std::string toLower(const std::string &s)
	{
		std::string out(s);
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
		return out;
	}

	std::string trim(const std::string &s)
	{
		std::string::size_type start = 0;
		std::string::size_type end = s.size();
		while (start < end && (s[start] == ' ' || s[start] == '\t'))
			++start;
		while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
			--end;
		return s.substr(start, end - start);
	}

	std::vector<std::string> splitCRLF(const std::string &block)
	{
		std::vector<std::string> out;
		std::string::size_type start = 0;
		while (true)
		{
			std::string::size_type pos = block.find("\r\n", start);
			if (pos == std::string::npos)
			{
				out.push_back(block.substr(start));
				break;
			}
			out.push_back(block.substr(start, pos - start));
			start = pos + 2;
		}
		return out;
	}

	// Returns: 0 = supported, 400 = malformed, 505 = unsupported version.
	int checkVersion(const std::string &v)
	{
		if (v.size() != 8 || v.compare(0, 5, "HTTP/") != 0)
			return 400;
		if (!std::isdigit(static_cast<unsigned char>(v[5])) ||
			v[6] != '.' ||
			!std::isdigit(static_cast<unsigned char>(v[7])))
			return 400;
		if (v == "HTTP/1.0" || v == "HTTP/1.1")
			return 0;
		return 505;
	}

	bool parseContentLength(const std::string &s, size_t &out)
	{
		if (s.empty())
			return false;
		size_t val = 0;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(s[i])))
				return false;
			val = val * 10 + static_cast<size_t>(s[i] - '0');
		}
		out = val;
		return true;
	}
}

// --- orthodox canonical form -----------------------------------------------

void HTTPRequest::init()
{
	_buffer.clear();
	_method.clear();
	_uri.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_headersParsed = false;
	_hasContentLength = false;
	_contentLength = 0;
	_maxBodySize = static_cast<size_t>(-1); // effectively unlimited until set
	_complete = false;
	_valid = true;
	_errorCode = 0;
}

HTTPRequest::HTTPRequest()
{
	init();
}

HTTPRequest::HTTPRequest(const HTTPRequest &other)
{
	*this = other;
}

HTTPRequest &HTTPRequest::operator=(const HTTPRequest &other)
{
	if (this != &other)
	{
		_buffer = other._buffer;
		_method = other._method;
		_uri = other._uri;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_headersParsed = other._headersParsed;
		_hasContentLength = other._hasContentLength;
		_contentLength = other._contentLength;
		_maxBodySize = other._maxBodySize;
		_complete = other._complete;
		_valid = other._valid;
		_errorCode = other._errorCode;
	}
	return *this;
}

HTTPRequest::~HTTPRequest()
{
}

// --- parsing ---------------------------------------------------------------

void HTTPRequest::setError(int code)
{
	_valid = false;
	_errorCode = code;
	_complete = true; // parsing is finished; server should send the error
}

void HTTPRequest::feed(const std::string &data)
{
	if (_complete || !_valid)
		return;
	_buffer += data;
	parse();
}

void HTTPRequest::parse()
{
	if (!_headersParsed)
	{
		std::string::size_type pos = _buffer.find("\r\n\r\n");
		if (pos == std::string::npos)
			return; // headers not fully received yet
		std::string block = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 4); // whatever remains is body bytes
		if (!parseHeaderBlock(block))
			return; // error already set
		_headersParsed = true;
	}
	if (_headersParsed && _valid)
		parseBody();
}

bool HTTPRequest::parseHeaderBlock(const std::string &block)
{
	std::vector<std::string> lines = splitCRLF(block);
	if (lines.empty())
	{
		setError(400);
		return false;
	}
	if (!parseRequestLine(lines[0]))
		return false;

	for (size_t i = 1; i < lines.size(); ++i)
	{
		const std::string &line = lines[i];
		if (line.empty())
			continue;
		std::string::size_type colon = line.find(':');
		if (colon == std::string::npos)
		{
			setError(400);
			return false;
		}
		std::string name = toLower(trim(line.substr(0, colon)));
		std::string value = trim(line.substr(colon + 1));
		if (name.empty())
		{
			setError(400);
			return false;
		}
		_headers[name] = value;
	}

	bool hasCL = _headers.find("content-length") != _headers.end();
	bool hasTE = _headers.find("transfer-encoding") != _headers.end();
	if (hasCL && hasTE)
	{
		setError(400);
		return false;
	}
	if (hasTE)
	{
		setError(501); // chunked / any transfer-encoding is out of scope
		return false;
	}
	if (hasCL)
	{
		size_t len;
		if (!parseContentLength(_headers["content-length"], len))
		{
			setError(400);
			return false;
		}
		if (len > _maxBodySize)
		{
			setError(413);
			return false;
		}
		_hasContentLength = true;
		_contentLength = len;
	}
	return true;
}

bool HTTPRequest::parseRequestLine(const std::string &line)
{
	std::string::size_type sp1 = line.find(' ');
	if (sp1 == std::string::npos)
	{
		setError(400);
		return false;
	}
	std::string::size_type sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos)
	{
		setError(400);
		return false;
	}
	_method = line.substr(0, sp1);
	_uri = line.substr(sp1 + 1, sp2 - sp1 - 1);
	_version = line.substr(sp2 + 1);

	if (_method.empty() || _uri.empty() || _version.empty() ||
		_version.find(' ') != std::string::npos)
	{
		setError(400);
		return false;
	}

	int vc = checkVersion(_version);
	if (vc != 0)
	{
		setError(vc);
		return false;
	}
	return true;
}

void HTTPRequest::parseBody()
{
	if (!_hasContentLength)
	{
		_complete = true; // no Content-Length => empty body, done after headers
		return;
	}
	if (_buffer.size() >= _contentLength)
	{
		_body = _buffer.substr(0, _contentLength);
		_buffer.erase(0, _contentLength);
		_complete = true;
	}
	// otherwise wait for the rest of the body
}

// --- accessors -------------------------------------------------------------

bool HTTPRequest::isComplete() const { return _complete; }
bool HTTPRequest::isValid() const { return _valid; }
int HTTPRequest::getErrorCode() const { return _errorCode; }

std::string HTTPRequest::getMethod() const { return _method; }
std::string HTTPRequest::getUri() const { return _uri; }
std::string HTTPRequest::getVersion() const { return _version; }
std::map<std::string, std::string> HTTPRequest::getHeaders() const { return _headers; }
std::string HTTPRequest::getBody() const { return _body; }

void HTTPRequest::setMaxBodySize(size_t max) { _maxBodySize = max; }
