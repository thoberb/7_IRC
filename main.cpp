/* ************************************************************************** */
/*																																						*/
/*																												:::		  ::::::::   */
/*   main.cpp																				   :+:		  :+:		:+:   */
/*																										+:+ +:+				 +:+		 */
/*   By: bberthod <bberthod@student.42.fr>				  +#+  +:+		   +#+				*/
/*																								+#+#+#+#+#+   +#+				   */
/*   Created: 2024/05/08 19:26:31 by bberthod				  #+#		#+#						 */
/*   Updated: 2024/05/09 17:04:16 by bberthod				 ###   ########.fr		   */
/*																																						*/
/* ************************************************************************** */

//http://dicoinformatique.chez.com/irc.htm#mode ---> Donne les commandes IRC
//https://modern.ircdocs.horse/ ---> Explique un peu tout

#include "ServerSocket.hpp"
#include <iostream>
#include <cstdlib> // Pour std::atoi

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = std::atoi(argv[1]);
	std::string password = argv[2];

	ServerSocket server(password);
	if (!server.setup(port))
	{
		std::cerr << "Failed to setup server" << std::endl;
		return 1;
	}
	std::cout << "Server running on port " << port << std::endl;
	server.run();

	return 0;
}
