/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:44:57 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/23 19:44:42 by halmuhis         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

// =============================================================================
// 1. Orthodox Canonical Form
// =============================================================================

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {}

Server::~Server()
{
	for (std::map<int, size_t>::iterator it = _listen_fds.begin(); it != _listen_fds.end(); ++it)
	{
		close(it->first);
	}
}

// =============================================================================
// 2. Helper Function
// =============================================================================

void Server::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("Failed to set non-blocking");
}

void Server::acceptNewClient(int listen_fd)
{
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
		return;
	setNonBlocking(client_fd);
	struct pollfd client_pfd;
	client_pfd.fd = client_fd;
	client_pfd.events = POLLIN;
	client_pfd.revents = 0;
	_poll_fds.push_back(client_pfd);

	Client new_client;
	new_client.fd = client_fd;
	new_client.write_buffer = "";
	new_client.server_index = _listen_fds[listen_fd];
	new_client.request.setMaxBodySize(_configs[new_client.server_index].client_max_body_size);
	_clients[client_fd] = new_client;
}

void Server::closeClient(size_t i)
{
	int fd = _poll_fds[i].fd;

	close(fd);
	_clients.erase(fd);
	_poll_fds.erase(_poll_fds.begin() + i);
}

bool Server::handleClientRead(size_t i)
{
	int fd = _poll_fds[i].fd;
	Client &client = _clients[fd];

	char buf[4096];
	ssize_t n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		closeClient(i);
		return false;
	}

	client.request.feed(std::string(buf, n));

	if (client.request.isComplete())
	{
		HTTPResponse response = buildResponse(_configs[client.server_index], client.request);
		client.write_buffer = response.toString();
		_poll_fds[i].events = POLLOUT;
	}
	return true;
}

bool Server::handleClientWrite(size_t i)
{
	int fd = _poll_fds[i].fd;
	Client &client = _clients[fd];

	ssize_t n = send(fd, client.write_buffer.c_str(), client.write_buffer.size(), 0);

	if (n <= 0)
	{
		closeClient(i);
		return false;
	}

	client.write_buffer.erase(0, n);

	if (client.write_buffer.empty())
	{
		closeClient(i);
		return false;
	}

	return true;
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

std::string Server::getContentType(const std::string &path)
{
	std::string::size_type dot = path.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(dot);
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".css")
		return "text/css";
	if (ext == ".js")
		return "application/javascript";
	if (ext == ".png")
		return "image/png";
	if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	if (ext == ".gif")
		return "image/gif";
	if (ext == ".txt")
		return "text/plain";
	return "application/octet-stream";
}

bool Server::readFile(const std::string &path, std::string &out)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;
	std::stringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

HTTPResponse Server::buildResponse(const ServerConfig &server, const HTTPRequest &request)
{
	HTTPResponse response;

	if (!request.isValid())
		return makeErrorResponse(server, request.getErrorCode());

	const LocationConfig *matched_loc = matchLocation(server, request.getUri());
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

	std::string real_filepath = resolvePath(*matched_loc, request.getUri());

	struct stat st;
	if (stat(real_filepath.c_str(), &st) != 0)
	{
		response.setStatus(404);
	}
	else if (S_ISDIR(st.st_mode))
	{
		std::string index_path = real_filepath;
		if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
			index_path += '/';
		index_path += matched_loc->index;

		std::string file_content;
		if (!matched_loc->index.empty() && readFile(index_path, file_content))
		{
			response.setStatus(200);
			response.setBody(file_content);
			response.setHeader("Content-Type", getContentType(index_path));
		}
		else if (matched_loc->autoindex)
		{
			response.setStatus(200);
			response.setBody(generateAutoindex(real_filepath, request.getUri()));
			response.setHeader("Content-Type", "text/html");
		}
		else
		{
			response.setStatus(403);
		}
	}
	else
	{
		std::string file_content;
		if (!readFile(real_filepath, file_content))
		{
			response.setStatus(403);
		}
		else
		{
			response.setStatus(200);
			response.setBody(file_content);
			response.setHeader("Content-Type", getContentType(real_filepath));
		}
	}

	if (response.getStatus() >= 400)
	{
		response.setBody(getErrorBody(server, response.getStatus()));
		response.setHeader("Content-Type", "text/html");
	}

	return response;
}

std::string Server::generateAutoindex(const std::string &dir_path, const std::string &uri)
{
	DIR *dir = opendir(dir_path.c_str());
	if (dir == NULL)
	{
		return "<html><body><h1>Error 500</h1><p>Cannot open directory</p></body></html>";
	}

	std::stringstream ss;

	std::string base_uri = uri;
	if (base_uri.empty() || base_uri[base_uri.size() - 1] != '/')
		base_uri += '/';

	ss << "<html>\n<head>\n<title>Index of " << uri << "</title>\n";
	ss << "<style>\n"
	   << "  body { font-family: monospace; margin: 20px; background-color: #f8f9fa; }\n"
	   << "  h1 { color: #212529; }\n"
	   << "  hr { border: 0; border-top: 1px solid #dee2e6; margin: 10px 0; }\n"
	   << "  ul { list-style-type: none; padding-left: 5px; }\n"
	   << "  li { margin: 6px 0; font-size: 16px; }\n"
	   << "  a { color: #0056b3; text-decoration: none; }\n"
	   << "  a:hover { text-decoration: underline; }\n"
	   << "</style>\n</head>\n<body>\n";

	ss << "<h1>Index of " << uri << "</h1>\n<hr>\n<ul>\n";

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;

		if (name == ".")
			continue;

		std::string href = base_uri + name;

		ss << "    <li><a href=\"" << href << "\">" << name << "</a></li>\n";
	}
	closedir(dir);

	ss << "</ul>\n<hr>\n</body>\n</html>\n";

	return ss.str();
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
		{
			return file_content;
		}
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
	{
		status_text = numToString(code) + " Error";
		break;
	}
	}

	return "<html><head><title>" + status_text + "</title></head>"
												 "<body><center><h1>" +
		   status_text + "</h1></center><hr><center>webserv/1.0</center></body></html>";
}

HTTPResponse Server::makeErrorResponse(const ServerConfig &server, int code)
{
	HTTPResponse response;

	response.setStatus(code);
	response.setBody(getErrorBody(server, code));
	response.setHeader("Content-Type", "text/html");

	return response;
}

// =============================================================================
// 3. Utility Functions
// =============================================================================

std::string Server::numToString(int number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

// =============================================================================
// 4. Main Functions
// =============================================================================

void Server::setup()
{
	int opt = 1;
	for (size_t i = 0; i < _configs.size(); i++)
	{

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			throw std::runtime_error("Failed to create socket");

		_listen_fds[fd] = i;

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			throw std::runtime_error("Failed to set SO_REUSEADDR");
		}

		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(_configs[i].port);

		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			throw std::runtime_error("Failed to bind socket");
		}

		if (listen(fd, SOMAXCONN) < 0)
		{
			throw std::runtime_error("Failed to listen on socket");
		}

		setNonBlocking(fd);

		struct pollfd listen_pfd;
		listen_pfd.fd = fd;
		listen_pfd.events = POLLIN;
		listen_pfd.revents = 0;
		_poll_fds.push_back(listen_pfd);
	}
}

void Server::run()
{
	while (true)
	{
		int ret = poll(&_poll_fds[0], _poll_fds.size(), -1);

		if (ret < 0)
			continue;

		for (size_t i = 0; i < _poll_fds.size(); i++)
		{
			if (_poll_fds[i].revents == 0)
				continue;

			if (_listen_fds.count(_poll_fds[i].fd))
			{
				if (_poll_fds[i].revents & POLLIN)
					acceptNewClient(_poll_fds[i].fd);
			}
			else
			{
				if (_poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					closeClient(i);
					i--;
					continue;
				}

				if (_poll_fds[i].revents & POLLIN)
				{
					if (!handleClientRead(i))
					{
						i--;
						continue;
					}
				}

				if (_poll_fds[i].revents & POLLOUT)
				{
					if (!handleClientWrite(i))
					{
						i--;
						continue;
					}
				}
			}
		}
	}
}