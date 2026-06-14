/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:36:20 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/15 01:46:56 by halmuhis         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>

class Server
{
private:
	int _listen_fd;
	int _port;
	struct sockaddr_in _addr;

	Server(const Server &other);
	Server &operator=(const Server &other);

public:
	Server(int port);
	~Server();

	void setup();
	void run();
};

#endif