/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:44:57 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/15 02:01:28 by halmuhis         ###   ########.fr       */
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

Server::Server(int port) : _listen_fd(-1), _port(port) {}
Server::~Server() {}

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
}

void Server::run()
{
	while (true)
	{
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(_listen_fd, (struct sockaddr *)&client_addr, &client_len);
		if (client_fd < 0)
			continue;
		
		std::cout << "Client connected!" << std::endl;

		char buffer[4096];
		ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
		if (bytes_received > 0)
		{
			buffer[bytes_received] = '\0';
			std::cout << "Received: " << buffer << std::endl;
			const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
			send(client_fd, response, std::strlen(response), 0);
		}
		close(client_fd);
	}
}