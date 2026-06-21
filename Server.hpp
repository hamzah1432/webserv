/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/21 14:48:24 by halmuhis         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>
#include <fcntl.h>
#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include "Config.hpp"

struct Client
{
	int fd;
	std::string read_buffer;
	std::string write_buffer;
	size_t server_index;
};

class Server
{
private:
	std::vector<struct pollfd> _poll_fds;
	std::map<int, Client> _clients;
	std::vector<ServerConfig> _configs;
	std::map<int, size_t> _listen_fds;

	// Orthodox Canonical Form
	Server(const Server &other);
	Server &operator=(const Server &other);
	// Helper Functions
	void setNonBlocking(int fd);
	void acceptNewClient(int listen_fd);
	void closeClient(size_t i);
	bool handleClientRead(size_t i);
	bool handleClientWrite(size_t i);

public:
	Server(const std::vector<ServerConfig> &configs);
	~Server();
	void setup();
	void run();
};

#endif