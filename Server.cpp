/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:44:57 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/21 19:48:39 by halmuhis         ###   ########.fr       */
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
#include <cstring>

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
		HTTPResponse response;

		if (!client.request.isValid())
		{
			response.setStatus(client.request.getErrorCode());
			response.setBody("Error: Bad Request\n");
		}
		else
		{
			const ServerConfig &current_server = _configs[client.server_index];
			const LocationConfig *matched_loc = matchLocation(current_server, client.request.getUri());

			if (matched_loc == NULL)
			{
				response.setStatus(404);
				response.setBody("Error 404: Not Found\n");
			}
			else
			{
				std::string real_filepath = resolvePath(*matched_loc, client.request.getUri());

				std::ifstream file(real_filepath.c_str(), std::ios::binary);
				if (!file.is_open())
				{
					response.setStatus(404);
					response.setBody("Error 404: Not Found\n");
				}
				else
				{
					std::stringstream ss;
					ss << file.rdbuf();
					response.setStatus(200);
					response.setBody(ss.str());
					response.setHeader("Content-Type", getContentType(real_filepath));
				}
			}
		}

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

	std::string ext = path.substr(dot); // includes the dot, e.g. ".html"
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

// =============================================================================
// 3. Main Functions
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