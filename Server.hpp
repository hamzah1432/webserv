/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/29 16:13:39 by halmuhis         ###   ########.fr       */
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
#include <csignal>
#include <sys/wait.h>


enum ClientState
{
	READING,
	CGI_RUNNING,
	WRITING
};

struct CgiProcess
{
	pid_t pid;
	int stdin_fd;
	int stdout_fd;
	std::string input;
	size_t input_sent;
	std::string output;

	CgiProcess() : pid(-1), stdin_fd(-1), stdout_fd(-1), input_sent(0) {}
};

struct Client
{
	int fd;
	std::string write_buffer;
	size_t server_index;
	HTTPRequest request;
	ClientState state;
	CgiProcess cgi;
	time_t last_activity;
	Client() : fd(-1), server_index(0), state(READING), last_activity(time(0)) {}
};

class Server
{
private:
	std::vector<struct pollfd> _poll_fds;
	std::map<int, Client> _clients;
	std::vector<ServerConfig> _configs;
	std::map<int, size_t> _listen_fds;
	std::map<int, int> _cgi_fds;

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
	bool writeFile(const std::string &path, const std::string &content);

	HTTPResponse handleGet(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request);
	HTTPResponse handlePost(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request);
	HTTPResponse handleDelete(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request);

	bool isCgiRequest(const LocationConfig &loc, const std::string &filepath);
	void startCgi(Client &client, const LocationConfig &loc, const std::string &filepath);
	std::vector<std::string> buildCgiEnv(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request, const std::string &filepath);
	bool handleCgiIo(size_t i);
	void removeCgiFd(size_t i);
	HTTPResponse parseCgiOutput(const std::string &raw);

	// Utility Functions
	std::string numToString(int number);
	void checkTimeouts();
	void removePollFd(int fd);
	void closeClientFull(int client_fd);

public:
	Server(const std::vector<ServerConfig> &configs);
	~Server();
	void setup();
	void run();
};

#endif