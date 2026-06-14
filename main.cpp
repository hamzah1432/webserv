/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: halmuhis <halmuhesn@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/14 16:49:59 by halmuhis          #+#    #+#             */
/*   Updated: 2026/06/15 01:30:16 by halmuhis         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
int main()
{
	Server server(8080);
	try
	{
		server.setup();
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << '\n';
	}

	return 0;
}