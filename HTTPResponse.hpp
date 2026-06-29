#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HTTPResponse
{
private:
	int _statusCode;

	std::string _reasonPhrase;

	std::map<std::string, std::string> _headers;

	std::string _body;

	std::string getReasonPhrase(int code) const;

public:
	HTTPResponse();

	void setStatus(int code);

	int getStatusCode() const;
	std::string getHeader(const std::string &key) const;
	bool hasHeader(const std::string &key) const;
	const std::string &getBody() const;

	void setHeader(
		const std::string &key,
		const std::string &value);

	void setBody(const std::string &body);

	void removeHeader(const std::string &key);

	void setContentType(
		const std::string &type);

	void setRedirect(
		int code,
		const std::string &location);

	void reset();

	std::string build() const;

};

#endif