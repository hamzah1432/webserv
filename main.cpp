/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:49:59 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/16 08:51:48 by halmuhis         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "Config.hpp"
#include <iostream>

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		std::cerr << "Usage: ./webserv [configuration file]" << '\n';
		return 1;
	}

	std::string path = (argc == 2) ? argv[1] : "config/default.conf";

	try
	{
		Config config;
		std::vector<ServerConfig> servers = config.parse(path);

		// Day 3 will host every server; for now we launch the first one.
		Server server(servers[0].port);
		server.setup();
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}

	return 0;
}
