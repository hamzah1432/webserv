/*temp*/

#include "HTTPResponse.hpp"
#include <sstream>
#include <cctype>


namespace
{
    std::string toLower(const std::string &s)
    {
        std::string out(s);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
        return out;
    }

    std::string reasonPhrase(int code)
    {
        switch (code)
        {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 408: return "Request Timeout";
            case 411: return "Length Required";
            case 413: return "Payload Too Large";
            case 414: return "URI Too Long";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 505: return "HTTP Version Not Supported";
            default:  return "Unknown";
        }
    }
}

// --- orthodox canonical form -----------------------------------------------

HTTPResponse::HTTPResponse()
    : _code(200), _reason("OK"), _headers(), _body()
{
}

HTTPResponse::HTTPResponse(const HTTPResponse &other)
{
    *this = other;
}

HTTPResponse &HTTPResponse::operator=(const HTTPResponse &other)
{
    if (this != &other)
    {
        _code = other._code;
        _reason = other._reason;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}

HTTPResponse::~HTTPResponse()
{
}

// --- builders --------------------------------------------------------------

void HTTPResponse::setStatus(int code)
{
    _code = code;
    _reason = reasonPhrase(code);
}

void HTTPResponse::setHeader(const std::string &key, const std::string &value)
{
    _headers[key] = value;
}

void HTTPResponse::setBody(const std::string &body)
{
    _body = body;
}

std::string HTTPResponse::toString() const
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << _code << " " << _reason << "\r\n";

    // Emit user headers, but never the ones we always set ourselves.
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it)
    {
        std::string lname = toLower(it->first);
        if (lname == "content-length" || lname == "connection")
            continue;
        oss << it->first << ": " << it->second << "\r\n";
    }

    oss << "Content-Length: " << _body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << _body;

    return oss.str();
}
