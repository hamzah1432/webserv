/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/16 08:19:53 by halmuhis         ###   ########.fr       */
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

struct Client
{
	int fd;
	std::string read_buffer;
	std::string write_buffer;
};

class Server
{
private:
	int _listen_fd;
	int _port;
	struct sockaddr_in _addr;
	std::vector<struct pollfd> _poll_fds;
	std::map<int, Client> _clients;
	// Orthodox Canonical Form
	Server(const Server &other);
	Server &operator=(const Server &other);
	// Helper Functions
	void setNonBlocking(int fd);
	void acceptNewClient();
	void closeClient(size_t i);

public:
	Server(int port);
	~Server();
	void setup();
	void run();
};

#endif