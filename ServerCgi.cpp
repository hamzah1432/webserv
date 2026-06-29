#include "Server.hpp"
#include <sys/wait.h>
#include <cstdlib>

bool Server::isCgiRequest(const LocationConfig &loc, const std::string &filepath)
{
	if (loc.cgi_extension.empty() || loc.cgi_path.empty())
		return false;
	if (filepath.size() < loc.cgi_extension.size())
		return false;
	return filepath.compare(filepath.size() - loc.cgi_extension.size(),
							loc.cgi_extension.size(), loc.cgi_extension) == 0;
}

void Server::startCgi(Client &client, const LocationConfig &loc, const std::string &filepath)
{
	int in_pipe[2];
	int out_pipe[2];

	if (pipe(in_pipe) < 0)
	{
		HTTPResponse r = makeErrorResponse(_configs[client.server_index], 500);
		client.write_buffer = r.build();
		client.state = WRITING;
		return;
	}
	if (pipe(out_pipe) < 0)
	{
		close(in_pipe[0]);
		close(in_pipe[1]);
		HTTPResponse r = makeErrorResponse(_configs[client.server_index], 500);
		client.write_buffer = r.build();
		client.state = WRITING;
		return;
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		HTTPResponse r = makeErrorResponse(_configs[client.server_index], 500);
		client.write_buffer = r.build();
		client.state = WRITING;
		return;
	}

	if (pid == 0)
	{
		// child
		dup2(in_pipe[0], STDIN_FILENO);
		dup2(out_pipe[1], STDOUT_FILENO);
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);

		std::string dir = filepath;
		std::string script = filepath;
		std::string::size_type slash = filepath.rfind('/');
		if (slash != std::string::npos)
		{
			dir = filepath.substr(0, slash);
			script = filepath.substr(slash + 1);
			if (!dir.empty())
				chdir(dir.c_str());
		}

		std::vector<std::string> env = buildCgiEnv(_configs[client.server_index], loc, client.request, filepath);

		char *argv[3];
		argv[0] = const_cast<char *>(loc.cgi_path.c_str());
		argv[1] = const_cast<char *>(script.c_str());
		argv[2] = NULL;

		std::vector<char *> envp;
		for (size_t k = 0; k < env.size(); ++k)
			envp.push_back(const_cast<char *>(env[k].c_str()));
		envp.push_back(NULL);

		execve(loc.cgi_path.c_str(), argv, &envp[0]);
		exit(1);
	}
	// parent
	close(in_pipe[0]);
	close(out_pipe[1]);

	setNonBlocking(in_pipe[1]);
	setNonBlocking(out_pipe[0]);

	client.cgi.pid = pid;
	client.cgi.stdin_fd = in_pipe[1];
	client.cgi.stdout_fd = out_pipe[0];
	client.cgi.input = client.request.getBody();
	client.cgi.input_sent = 0;
	client.cgi.output = "";
	client.state = CGI_RUNNING;

	struct pollfd pin;
	pin.fd = in_pipe[1];
	pin.events = POLLOUT;
	pin.revents = 0;
	_poll_fds.push_back(pin);

	struct pollfd pout;
	pout.fd = out_pipe[0];
	pout.events = POLLIN;
	pout.revents = 0;
	_poll_fds.push_back(pout);

	_cgi_fds[in_pipe[1]] = client.fd;
	_cgi_fds[out_pipe[0]] = client.fd;
}

std::vector<std::string> Server::buildCgiEnv(const ServerConfig &server, const LocationConfig &loc, const HTTPRequest &request, const std::string &filepath)
{
	(void)loc;
	std::vector<std::string> env;

	std::string uri = request.getURI();
	std::string script_name = uri;
	std::string query = "";
	std::string::size_type q = uri.find('?');
	if (q != std::string::npos)
	{
		script_name = uri.substr(0, q);
		query = uri.substr(q + 1);
	}

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("REDIRECT_STATUS=200");

	env.push_back("REQUEST_METHOD=" + request.getMethod());
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("QUERY_STRING=" + query);

	env.push_back("SCRIPT_NAME=" + script_name);
	env.push_back("SCRIPT_FILENAME=" + filepath);
	env.push_back("PATH_INFO=" + filepath);

	std::string server_name = server.server_names.empty() ? server.host : server.server_names[0];
	env.push_back("SERVER_NAME=" + server_name);
	env.push_back("SERVER_PORT=" + numToString(server.port));

	env.push_back("CONTENT_LENGTH=" + numToString(request.getBody().size()));

	std::map<std::string, std::string> headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator ct = headers.find("content-type");
	if (ct != headers.end())
		env.push_back("CONTENT_TYPE=" + ct->second);

	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		if (it->first == "content-type" || it->first == "content-length")
			continue;
		std::string key = it->first;
		for (size_t k = 0; k < key.size(); ++k)
		{
			if (key[k] == '-')
				key[k] = '_';
			else
				key[k] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[k])));
		}
		env.push_back("HTTP_" + key + "=" + it->second);
	}

	return env;
}

bool Server::handleCgiIo(size_t i)
{
	int fd = _poll_fds[i].fd;
	int client_fd = _cgi_fds[fd];
	Client &client = _clients[client_fd];

	if (fd == client.cgi.stdin_fd && (_poll_fds[i].revents & POLLOUT))
	{
		const std::string &in = client.cgi.input;
		if (client.cgi.input_sent < in.size())
		{
			ssize_t n = write(fd, in.c_str() + client.cgi.input_sent,
							  in.size() - client.cgi.input_sent);
			if (n > 0)
				client.cgi.input_sent += n;
			else
			{

				removeCgiFd(i);
				client.cgi.stdin_fd = -1;
				return false;
			}
		}
		if (client.cgi.input_sent >= in.size())
		{
			removeCgiFd(i);
			client.cgi.stdin_fd = -1;
			return false;
		}
		return true;
	}

	if (fd == client.cgi.stdout_fd && (_poll_fds[i].revents & (POLLIN | POLLHUP)))
	{
		char buf[4096];
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n > 0)
		{
			client.cgi.output.append(buf, n);
			return true;
		}

		int status;
		waitpid(client.cgi.pid, &status, 0);

		HTTPResponse response = parseCgiOutput(client.cgi.output);
		client.write_buffer = response.build();
		client.state = WRITING;


		for (size_t k = 0; k < _poll_fds.size(); ++k)
		{
			if (_poll_fds[k].fd == client.fd)
			{
				_poll_fds[k].events = POLLOUT;
				break;
			}
		}

		removeCgiFd(i);
		client.cgi.stdout_fd = -1;
		return false;
	}

	return true;
}

void Server::removeCgiFd(size_t i)
{
	int fd = _poll_fds[i].fd;
	close(fd);
	_cgi_fds.erase(fd);
	_poll_fds.erase(_poll_fds.begin() + i);
}

HTTPResponse Server::parseCgiOutput(const std::string &raw)
{
	HTTPResponse response;
	response.setStatus(200);

	std::string::size_type sep = raw.find("\r\n\r\n");
	size_t skip = 4;
	if (sep == std::string::npos)
	{
		sep = raw.find("\n\n");
		skip = 2;
	}

	if (sep == std::string::npos)
	{
		response.setBody(raw);
		response.setHeader("Content-Type", "text/html");
		return response;
	}

	std::string header_block = raw.substr(0, sep);
	std::string body = raw.substr(sep + skip);
	response.setBody(body);

	std::string::size_type start = 0;
	while (start < header_block.size())
	{
		std::string::size_type nl = header_block.find('\n', start);
		std::string line = (nl == std::string::npos)
							   ? header_block.substr(start)
							   : header_block.substr(start, nl - start);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		std::string::size_type colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string val = line.substr(colon + 1);
			while (!val.empty() && val[0] == ' ')
				val.erase(0, 1);
			response.setHeader(key, val);
		}
		if (nl == std::string::npos)
			break;
		start = nl + 1;
	}

	return response;
}