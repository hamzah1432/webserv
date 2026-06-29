#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HTTPRequest
{
private:
	std::string _buffer;

	bool _headersParsed;
	bool _complete;
	bool _valid;

	int _errorCode;

	size_t _contentLength;

	std::string _method;
	std::string _uri;
	std::string _version;

	std::map<std::string, std::string> _headers;

	std::string _body;

	void parseRequestLine(const std::string &line);
	void parseHeadersBlock(const std::string &headersBlock);

	bool validateMethod();
	bool validateVersion();
	bool validateContentLength();
	bool validateRequestLine();

public:
	HTTPRequest();

	void feed(const std::string &data);

	bool isComplete() const;

	bool isValid() const;
	int getErrorCode() const;

	bool hasHeader(const std::string &key) const;

	const std::string &getMethod() const;
	const std::string &getURI() const;
	const std::string &getVersion() const;

	const std::string &getBody() const;

	const std::map<std::string, std::string> &getHeaders() const;

	std::string getHeader(const std::string &key) const;

	std::string getPath() const;
	std::string getQueryString() const;

	bool shouldKeepAlive() const;

	void reset();

};

#endif