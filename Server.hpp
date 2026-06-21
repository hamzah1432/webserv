/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/21 20:12:38 by halmuhis         ###   ########.fr       */
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
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <fstream>
#include <sstream>

struct Client
{
	int fd;
	std::string write_buffer;
	size_t server_index;
	HTTPRequest request;
	Client() : fd(-1), server_index(0) {}
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
	const LocationConfig *matchLocation(const ServerConfig &server, const std::string &uri);
	std::string resolvePath(const LocationConfig &loc, const std::string &uri);
	std::string getContentType(const std::string &path);
public:
	Server(const std::vector<ServerConfig> &configs);
	~Server();
	void setup();
	void run();
};

#endif