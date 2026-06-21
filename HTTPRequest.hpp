/*temp*/
#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <cstddef>

class HTTPRequest
{
private:
	std::string _buffer;
	std::string _method;
	std::string _uri;
	std::string _version;
	std::map<std::string, std::string> _headers;
	std::string _body;

	bool _headersParsed;
	bool _hasContentLength;
	size_t _contentLength;
	size_t _maxBodySize;

	bool _complete;
	bool _valid;
	int _errorCode;

	void init();
	void setError(int code);
	void parse();
	bool parseHeaderBlock(const std::string &block);
	bool parseRequestLine(const std::string &line);
	void parseBody();

public:
	HTTPRequest();
	HTTPRequest(const HTTPRequest &other);
	HTTPRequest &operator=(const HTTPRequest &other);
	~HTTPRequest();

	void feed(const std::string &data);
	bool isComplete() const;
	bool isValid() const;
	int getErrorCode() const;

	std::string getMethod() const;
	std::string getUri() const;
	std::string getVersion() const;
	std::map<std::string, std::string> getHeaders() const;
	std::string getBody() const;

	void setMaxBodySize(size_t max);
};

#endif
