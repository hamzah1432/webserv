/*temp*/
#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HTTPResponse
{
private:
    int                                _code;
    std::string                        _reason;
    std::map<std::string, std::string> _headers;
    std::string                        _body;

public:
    HTTPResponse();
    HTTPResponse(const HTTPResponse &other);
    HTTPResponse &operator=(const HTTPResponse &other);
    ~HTTPResponse();

    void setStatus(int code);
    void setHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &body);

    std::string toString() const;
};

#endif
