/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:44:57 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/16 08:23:56 by halmuhis         ###   ########.fr       */
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

Server::Server(int port) : _listen_fd(-1), _port(port) {}
Server::~Server()
{
	if (_listen_fd >= 0)
		close(_listen_fd);
}

// =============================================================================
// 2. Helper Function
// =============================================================================

void Server::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("Failed to set non-blocking");
}

void Server::acceptNewClient()
{
	int client_fd = accept(_listen_fd, NULL, NULL);
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
	new_client.read_buffer = "";
	new_client.write_buffer = "";
	_clients[client_fd] = new_client;
}

void Server::closeClient(size_t i)
{
	int fd = _poll_fds[i].fd;

	close(fd);
	_clients.erase(fd);
	_poll_fds.erase(_poll_fds.begin() + i);
}

// =============================================================================
// 3. Main Functions
// =============================================================================

void Server::setup()
{
	int opt = 1;
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (_listen_fd < 0)
	{
		throw std::runtime_error("Failed to create socket");
	}

	if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		throw std::runtime_error("Failed to set SO_REUSEADDR");
	}

	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = INADDR_ANY;
	_addr.sin_port = htons(_port);

	if (bind(_listen_fd, (struct sockaddr *)&_addr, sizeof(_addr)) < 0)
	{
		throw std::runtime_error("Failed to bind socket");
	}

	if (listen(_listen_fd, SOMAXCONN) < 0)
	{
		throw std::runtime_error("Failed to listen on socket");
	}

	setNonBlocking(_listen_fd);

	struct pollfd listen_pfd;
	listen_pfd.fd = _listen_fd;
	listen_pfd.events = POLLIN;
	listen_pfd.revents = 0;
	_poll_fds.push_back(listen_pfd);
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

			if (_poll_fds[i].fd == _listen_fd)
			{
				if (_poll_fds[i].revents & POLLIN)
				{
					acceptNewClient();
				}
			}
			else
			{
				int fd = _poll_fds[i].fd;
				Client &client = _clients[fd];

				if (_poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					closeClient(i);
					i--;
					continue;
				}

				if (_poll_fds[i].revents & POLLIN)
				{
					char buf[4096];
					ssize_t n = recv(fd, buf, sizeof(buf), 0);

					if (n <= 0)
					{
						closeClient(i);
						i--;
						continue;
					}
					else
					{
						client.read_buffer.append(buf, n);

						if (client.read_buffer.find("\r\n\r\n") != std::string::npos)
						{
							std::string response = "HTTP/1.1 200 OK\r\n"
												   "Content-Type: text/plain\r\n"
												   "Content-Length: 13\r\n"
												   "Connection: close\r\n\r\n"
												   "Hello, world!";

							client.write_buffer = response;
							_poll_fds[i].events = POLLIN | POLLOUT;
							client.read_buffer.clear();
						}
					}
				}

				if (_poll_fds[i].revents & POLLOUT)
				{
					ssize_t n = send(fd, client.write_buffer.c_str(), client.write_buffer.size(), 0);

					if (n <= 0)
					{
						closeClient(i);
						i--;
						continue;
					}
					else
					{
						client.write_buffer.erase(0, n);

						if (client.write_buffer.empty())
						{
							closeClient(i);
							i--;
							continue;
						}
					}
				}
			}
		}
	}
}