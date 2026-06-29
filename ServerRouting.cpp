#include "Server.hpp"
#include <algorithm>

HTTPResponse Server::buildResponse(const ServerConfig &server, const HTTPRequest &request)
{
	HTTPResponse response;

	if (!request.isValid())
		return makeErrorResponse(server, request.getErrorCode());

	const LocationConfig *matched_loc = matchLocation(server, request.getPath());
	if (matched_loc == NULL)
		return makeErrorResponse(server, 404);

	if (!checkMethodAllowed(matched_loc->methods, request.getMethod()))
	{
		std::string allowed_methods = "";
		for (size_t i = 0; i < matched_loc->methods.size(); ++i)
		{
			allowed_methods += matched_loc->methods[i];
			if (i < matched_loc->methods.size() - 1)
				allowed_methods += ", ";
		}

		response = makeErrorResponse(server, 405);
		response.setHeader("Allow", allowed_methods);
		return response;
	}

	if (!matched_loc->redirect.empty())
	{
		response.setStatus(301);
		response.setHeader("Location", matched_loc->redirect);
		return response;
	}

	std::string method = request.getMethod();
	if (method == "GET")
		return handleGet(server, *matched_loc, request);
	if (method == "POST")
		return handlePost(server, *matched_loc, request);
	if (method == "DELETE")
		return handleDelete(server, *matched_loc, request);

	return makeErrorResponse(server, 501);
}

const LocationConfig *Server::matchLocation(const ServerConfig &server, const std::string &uri)
{
	const LocationConfig *winner = NULL;
	size_t longest_match = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const LocationConfig &loc = server.locations[i];

		if (uri.compare(0, loc.path.size(), loc.path) == 0)
		{
			if (loc.path == "/" || uri.size() == loc.path.size() || uri[loc.path.size()] == '/')
			{
				if (loc.path.size() > longest_match)
				{
					longest_match = loc.path.size();
					winner = &loc;
				}
			}
		}
	}
	return winner;
}

std::string Server::resolvePath(const LocationConfig &loc, const std::string &uri)
{
	std::string remainder = uri.substr(loc.path.size());

	std::string root = loc.root;
	if (!root.empty() && root[root.size() - 1] == '/')
		root.erase(root.size() - 1);

	if (remainder.empty() || remainder[0] != '/')
		remainder = "/" + remainder;

	return root + remainder;
}

bool Server::checkMethodAllowed(const std::vector<std::string> &methods, const std::string &method)
{
	if (methods.empty())
		return true;

	return std::find(methods.begin(), methods.end(), method) != methods.end();
}

std::string Server::getErrorBody(const ServerConfig &server, int code)
{
	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);

	if (it != server.error_pages.end())
	{
		std::string file_content = "";
		if (readFile(it->second, file_content) && !file_content.empty())
			return file_content;
	}

	std::string status_text;
	switch (code)
	{
	case 400:
		status_text = "400 Bad Request";
		break;
	case 403:
		status_text = "403 Forbidden";
		break;
	case 404:
		status_text = "404 Not Found";
		break;
	case 405:
		status_text = "405 Method Not Allowed";
		break;
	case 413:
		status_text = "413 Payload Too Large";
		break;
	case 500:
		status_text = "500 Internal Server Error";
		break;
	case 501:
		status_text = "501 Not Implemented";
		break;
	case 505:
		status_text = "505 HTTP Version Not Supported";
		break;
	default:
		status_text = numToString(code) + " Error";
		break;
	}

	return "<html><head><title>" + status_text + "</title></head>"
												 "<body><center><h1>" +
		   status_text +
		   "</h1></center><hr><center>webserv/1.0</center></body></html>";
}

HTTPResponse Server::makeErrorResponse(const ServerConfig &server, int code)
{
	HTTPResponse response;

	response.setStatus(code);
	response.setBody(getErrorBody(server, code));
	response.setHeader("Content-Type", "text/html");

	return response;
}

std::string Server::numToString(int number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}