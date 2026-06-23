/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/23 23:34:01 by halmuhis         ###   ########.fr       */
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
#include <sys/stat.h>
#include <dirent.h>

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
	bool readFile(const std::string &path, std::string &out);
	HTTPResponse buildResponse(const ServerConfig &server, const HTTPRequest &request);
	std::string generateAutoindex(const std::string &dir_path, const std::string &uri);
	bool checkMethodAllowed(const std::vector<std::string> &methods, const std::string &method);
	std::string getErrorBody(const ServerConfig &server, int code);
	HTTPResponse makeErrorResponse(const ServerConfig &server, int code);

	HTTPResponse handleGet(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request);

	// Utility Functions
	std::string numToString(int number);

public:
	Server(const std::vector<ServerConfig> &configs);
	~Server();
	void setup();
	void run();
};

#endif