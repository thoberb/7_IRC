/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bberthod <bberthod@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/09 12:08:37 by bberthod          #+#    #+#             */
/*   Updated: 2024/07/16 16:29:53 by bberthod         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerSocket.hpp"
#include "Client.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h> // close
#include <arpa/inet.h> // inet_ntop
#include <poll.h>
#include <algorithm> // std::find_if
#include <csignal>

//----------------------CONSTRUCTOR-AND-DESTRUCTOR-----------------------------------------

ServerSocket* ServerSocket::_ptrServer = NULL;

ServerSocket::ServerSocket(const std::string& password) : server_password(password), server_socket(-1)
{
	std::memset(&server_addr, 0, sizeof(server_addr));
	_ptrServer = this;
}

ServerSocket::~ServerSocket()
{
	if (server_socket != -1)
		close(server_socket);
	for (std::vector<Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
		delete *it;
	for (std::vector<struct pollfd>::iterator it = poll_fds.begin(); it != poll_fds.begin(); it++)
		close(it->fd);
}

//----------------------SETUP-----------------------------------------

bool ServerSocket::setup(int port)
{
	std::cerr << "Setting up server on port " << port << std::endl;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		std::cerr << "Socket creation error" << std::endl;
		return false;
	}
	int opt = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::cerr << "Set socket options error" << std::endl;
		return false;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		std::cerr << "Bind error" << std::endl;
		return false;
	}
	if (listen(server_socket, 10) < 0)
	{
		std::cerr << "Listen error" << std::endl;
		return false;
	}
	struct pollfd server_pollfd;
	server_pollfd.fd = server_socket;
	server_pollfd.events = POLLIN;

	#ifdef __APPLE__
    // Définir le socket du serveur en mode non bloquant pour MacOS
    fcntl(server_socket, F_SETFL, O_NONBLOCK);
    #endif

	std::signal(SIGINT, closeServer);
	std::signal(SIGQUIT, closeServer);
	poll_fds.push_back(server_pollfd);
	std::cerr << "Server setup complete" << std::endl;
	return true;
}

void	ServerSocket::closeServer(int signal)
{
	(void)signal;
	close(_ptrServer->server_socket);
}

//----------------------ACCEPT-CONNECTION-----------------------------------------

int ServerSocket::acceptConnection()
{
	std::cerr << "Accepting new connection" << std::endl;
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
	if (client_socket < 0) {
		std::cerr << "Accept error" << std::endl;
		return -1;
	}

	#ifdef __APPLE__
    // Définir le socket client en mode non bloquant pour MacOS
    fcntl(client_socket, F_SETFL, O_NONBLOCK);
    #endif

	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(client_addr.sin_addr), str, INET_ADDRSTRLEN);
	std::string client_address(str);

	Client* new_client = new Client(client_socket, client_address);
	clients.push_back(new_client);

	struct pollfd client_pollfd;
	client_pollfd.fd = client_socket;
	client_pollfd.events = POLLIN;
	poll_fds.push_back(client_pollfd);

	std::cerr << "New connection accepted: " << client_address << std::endl;
	return client_socket;
}

//----------------------GET-SOCKET-----------------------------------------

int ServerSocket::getSocket() const
{
	return server_socket;
}

//----------------------HANDLE-CLIENT-----------------------------------------

void ServerSocket::handleClient(int index)
{
	std::cerr << "Handling client at index " << index << std::endl;
	if (index < 0 || static_cast<std::vector<Client*>::size_type>(index) >= clients.size())
	{
		std::cerr << "Invalid client index: " << index << std::endl;
		return;
	}
	char buffer[1024];
	int nbytes = recv(clients[index]->getFd(), buffer, sizeof(buffer), 0);
	if (nbytes <= 0)
	{
		std::cerr << "Client disconnected or recv error" << std::endl;
		removeClient(index);
	}
	else
	{
		std::string command(buffer, nbytes);
		std::cerr << "Received command: " << command << std::endl;
		handleCommand(index, command);
	}
}

//----------------------REMOVE-CLIENT-----------------------------------------

void ServerSocket::removeClient(int index)
{
	std::cerr << "Removing client at index " << index << std::endl;
	if (index < 0 || static_cast<std::vector<Client*>::size_type>(index) >= clients.size())
	{
		std::cerr << "Invalid client index: " << index << std::endl;
		return;
	}
	if (clients[index] != NULL)
	{
		close(clients[index]->getFd());
		delete clients[index];
		clients.erase(clients.begin() + index);
		poll_fds.erase(poll_fds.begin() + index + 1); // Ajuster pour le socket du serveur
	}
	else
	{
		std::cerr << "Client already removed" << std::endl;
	}
}

//----------------------SEND-TO-CLIENT-----------------------------------------

void ServerSocket::sendToClient(int index, const std::string& message)
{
	std::cerr << "Sending message to client at index " << index << ": " << message << std::endl;
	if (index < 0 || static_cast<std::vector<Client*>::size_type>(index) >= clients.size())
	{
		std::cerr << "Invalid client index: " << index << std::endl;
		return;
	}
	send(clients[index]->getFd(), message.c_str(), message.size(), 0);
}

//----------------------RUN-LOOP-----------------------------------------

void ServerSocket::run()
{
	while (true)
	{
		int poll_count = poll(poll_fds.data(), poll_fds.size(), -1);
		if (poll_count < 0)
		{
			std::cerr << "Poll error" << std::endl;
			break;
		}

		for (size_t i = 0; i < poll_fds.size(); ++i)
		{
			if (poll_fds[i].revents & POLLIN)
			{
				if (poll_fds[i].fd == server_socket)
				{
					acceptConnection();
				}
				else
				{
					int client_index = i - 1;
					handleClient(client_index);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//-----------------HANDLE-COMMAND-----------------------------------------

void ServerSocket::handleCommand(int client_index, const std::string& command)
{
	std::istringstream stream(command);
	std::string line;
	std::string previous_line;

	// Diviser la chaîne en plusieurs commandes si elle contient des nouvelles lignes
	while (std::getline(stream, line))
	{
		std::string	buffer = clients[client_index]->getBuffer();
		std::string new_line = line;
		if (line.empty())
			continue;
		if (!buffer.empty())
			line.insert(0, buffer);
		if (line[line.length() - 1] != '\r')
		{
			clients[client_index]->addToBuffer(new_line);
			continue;
		}
		else
			clients[client_index]->clearBuffer();
		if (line == previous_line)
		{
			continue; // Éviter les doublons
		}
		previous_line = line;
		std::cerr << "Received command: " << line << std::endl;
		std::istringstream iss(line);
		std::string cmd;
		std::vector<std::string> params;
		iss >> cmd;
		std::string param;
		while (iss >> param)
		{
			params.push_back(param);
		}

		if (cmd == "PASS")
		{
			commandPass(client_index, params);
			// Après avoir traité le PASS, vérifiez si le client peut être enregistré
			if (clients[client_index]->isNickSet() && clients[client_index]->isUserSet() && !clients[client_index]->isFullyRegistered() && clients[client_index]->isAuthenticated())
			{
				clients[client_index]->setRegistered(true);
				sendToClient(client_index, "001 " + clients[client_index]->getNickname() + " :Welcome to the IRC server\r\n");
				std::cerr << "Client fully registered after PASS command" << std::endl;
			}
			continue;
		}

		// Traiter les commandes NICK et USER en premier
		if (cmd == "NICK")
		{
			commandNick(client_index, params);
			// Après avoir traité le NICK, vérifiez si le client peut être enregistré
			if (clients[client_index]->isNickSet() && clients[client_index]->isUserSet() && !clients[client_index]->isFullyRegistered() && clients[client_index]->isAuthenticated())
			{
				clients[client_index]->setRegistered(true);
				sendToClient(client_index, "001 " + clients[client_index]->getNickname() + " :Welcome to the IRC server\r\n");
				std::cerr << "Client fully registered after NICK command" << std::endl;
			}
			continue;
		}
		else if (cmd == "USER")
		{
			commandUser(client_index, params);
			// Après avoir traité le USER, vérifiez si le client peut être enregistré
			if (clients[client_index]->isNickSet() && clients[client_index]->isUserSet() && !clients[client_index]->isFullyRegistered() && clients[client_index]->isAuthenticated())
			{
				clients[client_index]->setRegistered(true);
				sendToClient(client_index, "001 " + clients[client_index]->getNickname() + " :Welcome to the IRC server\r\n");
				std::cerr << "Client fully registered after USER command" << std::endl;
			}
			continue;
		}

		// Traiter la commande CAP
		if (cmd == "CAP")
		{
			if (params.size() > 0 && params[0] == "LS")
			{
				sendToClient(client_index, "CAP * LS :\r\n");
				continue;
			}
			else if (params.size() > 0 && params[0] == "END")
			{
				std::cerr << "Handling CAP END, checking registration status..." << std::endl;
				if (clients[client_index]->isNickSet() && clients[client_index]->isUserSet() && !clients[client_index]->isFullyRegistered() && clients[client_index]->isAuthenticated())
				{
					clients[client_index]->setRegistered(true);
					sendToClient(client_index, "001 " + clients[client_index]->getNickname() + " :Welcome to the IRC server\r\n");
					std::cerr << "Client fully registered after CAP END" << std::endl;
				}
				continue;
			}
		}

		// Vérifier si le client est enregistré
		if (!clients[client_index]->isAuthenticated())
		{
			sendToClient(client_index, "464 :Password required\r\n");
			continue;
		}

		if (!clients[client_index]->isFullyRegistered())
		{
			sendToClient(client_index, "451 :You have not registered\r\n");
			continue;
		}

		// Traiter les autres commandes
		if (cmd == "JOIN")
			commandJoin(client_index, params);
		else if (cmd == "PRIVMSG")
			commandPrivmsg(client_index, params);
		else if (cmd == "KICK")
			commandKick(client_index, params);
		else if (cmd == "INVITE")
			commandInvite(client_index, params);
		else if (cmd == "TOPIC")
			commandTopic(client_index, params);
		else if (cmd == "QUIT")
			commandQuit(client_index, params);
		else if (cmd == "MODE")
		{
			if (params.size() > 0 && clients[client_index]->getNickname() == params[0])
			{
				sendToClient(client_index, "MODE " + params[0] + " :" + params[1] + "\r\n");
			}
			else
			{
				// Traiter le mode canal
				commandMode(client_index, params);
			}
		}
		else if (cmd == "PING")
			sendToClient(client_index, "PONG " + params[0] + "\r\n");
		else
			std::cerr << "Unknown command: " << cmd << std::endl;
	}
}

//----------------------PASS----------------------------------------

void ServerSocket::commandPass(int client_index, const std::vector<std::string>& params)
{
	if (params.size() < 1) {
		sendToClient(client_index, "461 PASS :Not enough parameters\r\n");
		return;
	}

	std::string given_password = params[0];
	if (given_password == server_password)
	{
		clients[client_index]->setAuthenticated(true);
	}
	else
	{
		sendToClient(client_index, "464 :Password incorrect\r\n");
		removeClient(client_index);
	}
}

//----------------------NICK-----------------------------------------

std::string ServerSocket::generateUniqueNickname(const std::string& base_nickname)
{
	std::string unique_nickname = base_nickname;
	int suffix = 1;
	while (true) {
		bool nickname_exists = false;
		for (size_t i = 0; i < clients.size(); ++i) {
			if (clients[i]->getNickname() == unique_nickname) {
				nickname_exists = true;
				break;
			}
		}
		if (!nickname_exists) {
			break;
		}
		std::stringstream out;
		out << suffix;
		unique_nickname = base_nickname + out.str();
		++suffix;
	}
	return unique_nickname;
}

void ServerSocket::commandNick(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing NICK command" << std::endl;
	if (params.size() < 1)
	{
		sendToClient(client_index, "431 :No nickname given\r\n");
		return;
	}
	std::string new_nick = params[0];
	bool nickname_exists = false;

	for (size_t i = 0; i < clients.size(); ++i)
	{
		if (clients[i]->getNickname() == new_nick)
		{
			nickname_exists = true;
			break;
		}
	}

	if (nickname_exists)
	{
		new_nick = generateUniqueNickname(new_nick);
	}

	std::string old_nick = clients[client_index]->getNickname();
	clients[client_index]->setNickname(new_nick);
	std::string nick_message = ":" + old_nick + " NICK " + new_nick + "\r\n";

	sendToClient(client_index, nick_message);

	std::cerr << "NICK command processed: " << new_nick << std::endl;
}

//----------------------USER-----------------------------------------

void ServerSocket::commandUser(int client_index, const std::vector<std::string>& params)
{
	std::string RealName;

	std::cerr << "Processing USER command" << std::endl;
	if (params.size() < 4)
	{
		sendToClient(client_index, "461 USER :Not enough parameters\r\n");
		return;
	}

	clients[client_index]->setUsername(params[0]);
	std::string hostname = params[1];
	if (hostname.empty() || hostname == "NULL") {
		hostname = "default";
	}
	clients[client_index]->setHostname(hostname);
	for(int i = 3; !params[i].empty(); i++)
	{
		RealName += params[i] + ' ';
	}
	clients[client_index]->setRealname(RealName);

	sendToClient(client_index, ":localhost 001 " + clients[client_index]->getNickname() + " :Welcome to bdtServer " + clients[client_index]->getNickname() + "!~" + clients[client_index]->getUsername() + "@127.0.0.1\r\n");

	std::cerr << "USER command processed: " << params[0] << " " << params[1] << " " << RealName << std::endl;
}

//----------------------JOIN-----------------------------------------

void ServerSocket::commandJoin(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing JOIN command" << std::endl;
	if (params.size() < 1)
	{
		sendToClient(client_index, "461 JOIN :Not enough parameters\r\n");
		return;
	}

	std::string channel = params[0];
	std::string password = params.size() > 1 ? params[1] : "";

	// Vérifiez si le canal existe et initialisez-le si nécessaire
	if (channels.find(channel) == channels.end())
	{
		channels[channel] = std::vector<Client*>();
		channel_operators[channel].push_back(clients[client_index]);
	}

	// Vérifiez si l'utilisateur est déjà dans le canal
	if (std::find(channels[channel].begin(), channels[channel].end(), clients[client_index]) != channels[channel].end())
	{
		std::cout << "ERROR, on devrait pas etre la" << std::endl;
		return; // L'utilisateur est déjà dans le canal
	}

	// Vérifiez la limite du canal si le mode +l est activé
	if (std::find(channel_modes[channel].begin(), channel_modes[channel].end(), 'l') != channel_modes[channel].end())
	{
		if (channels[channel].size() >= static_cast<std::vector<Client*>::size_type>(channel_limits[channel]))
		{
			//pending_invites[channel].push_back(clients[client_index]); a activer avec celui dans INVITE pour se connecter direct apres invitation si on a deja tente
			sendToClient(client_index, "471 " + clients[client_index]->getNickname() + " " + channel + " :Cannot join channel (+l)\r\n");
			return;
		}
	}

	// Vérifiez si le canal est en mode +i
	if (std::find(channel_modes[channel].begin(), channel_modes[channel].end(), 'i') != channel_modes[channel].end())
	{
		if (std::find(channel_invitations[channel].begin(), channel_invitations[channel].end(), clients[client_index]->getNickname()) == channel_invitations[channel].end())
		{
			sendToClient(client_index, "473 " + clients[client_index]->getNickname() + " " + channel + " :Cannot join channel (+i)\r\n");
			return;
		}
		else
		{
			// Supprimez l'invitation une fois utilisée
			channel_invitations[channel].erase(std::remove(channel_invitations[channel].begin(), channel_invitations[channel].end(), clients[client_index]->getNickname()), channel_invitations[channel].end());
		}
	}

	// Vérifiez le mot de passe du canal si le mode +k est activé
	if (std::find(channel_modes[channel].begin(), channel_modes[channel].end(), 'k') != channel_modes[channel].end())
	{
		if (channel_passwords[channel] != password)
		{
			sendToClient(client_index, "475 " + clients[client_index]->getNickname() + " " + channel + " :Cannot join channel (+k)\r\n");
			return;
		}
	}

	// Ajouter l'utilisateur au canal
	channels[channel].push_back(clients[client_index]);
	std::string joinMessage = ":" + clients[client_index]->getNickname() + "!~ " + clients[client_index]->getUsername() + " JOIN :" + channel + "\r\n";
	sendToClient(client_index, joinMessage);


	// Envoyer le sujet actuel du canal au nouveau client
	std::map<std::string, std::string>::iterator topic_it = topics.find(channel);
	if (topic_it != topics.end())
		sendToClient(client_index, "332 " + clients[client_index]->getNickname() + " " + channel + " :" + topic_it->second + "\r\n");
	else
		sendToClient(client_index, "331 " + clients[client_index]->getNickname() + " " + channel + " :No topic is set\r\n");

	// Notifier tous les autres clients du canal que ce client a rejoint
	for (size_t i = 0; i < channels[channel].size(); ++i)
	{
		if (channels[channel][i] != clients[client_index])
		{
			sendToClient(std::distance(clients.begin(), std::find(clients.begin(), clients.end(), channels[channel][i])), joinMessage);
		}
	}
}

//----------------------PRIVMSG-----------------------------------------

void ServerSocket::commandPrivmsg(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing PRIVMSG command" << std::endl;
	if (params.size() < 2)
	{
		sendToClient(client_index, "461 PRIVMSG :Not enough parameters\r\n");
		return;
	}
	std::string target = params[0];
	std::string message = params[1];
	for (size_t i = 2; i < params.size(); ++i)
	{
		message += " " + params[i];
	}

	// Check if the target is a channel
	if (target[0] == '#')
	{
		std::map<std::string, std::vector<Client*> >::iterator it = channels.find(target);
		if (it != channels.end())
		{
			std::vector<Client*>& clients_in_channel = it->second;
			// Check if the client has joined the channel
			if (std::find(clients_in_channel.begin(), clients_in_channel.end(), clients[client_index]) != clients_in_channel.end())
			{
				for (std::vector<Client*>::iterator it = clients_in_channel.begin(); it != clients_in_channel.end(); ++it)
				{
					if (*it != clients[client_index])
					{
						sendToClient(std::distance(clients.begin(), std::find(clients.begin(), clients.end(), *it)), ":" + clients[client_index]->getNickname() + " PRIVMSG " + target + " :" + message + "\r\n");
					}
				}
			}
			else
			{
				sendToClient(client_index, "442 " + target + " :You're not on that channel\r\n");
			}
		}
		else
		{
			sendToClient(client_index, "403 " + target + " :No such channel\r\n");
		}
	}
	else
	{
		// Direct message to a user
		bool found = false;
		for (std::vector<Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
		{
			if ((*it)->getNickname() == target) {
				sendToClient(std::distance(clients.begin(), it), ":" + clients[client_index]->getNickname() + " PRIVMSG " + target + " :" + message + "\r\n");
				found = true;
				break;
			}
		}
		if (!found)
		{
			sendToClient(client_index, "401 " + target + " :No such nick\r\n");
		}
	}
}

//-------------------FIND-NICKNAMES-----------------------------------------

bool ServerSocket::nicknameMatches(Client* client, const std::string& nickname)
{
	return client->getNickname() == nickname;
}

Client* ServerSocket::findClientByNickname(const std::string& nickname)
{
	for (std::vector<Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
	{
		if ((*it)->getNickname() == nickname)
		{
			return *it;
		}
	}
	return NULL;
}

//----------------------KICK-----------------------------------------

void ServerSocket::commandKick(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing KICK command" << std::endl;
	if (params.size() < 2)
	{
		sendToClient(client_index, "461 KICK :Not enough parameters\r\n");
		return;
	}
	std::string channel = params[0];
	std::string target_nick = params[1];
	if (!isClientAutorize(channel_operators[channel], clients[client_index]))
	{
		sendToClient(client_index, "482 " + channel + " :You're not channel operator\r\n");
		sendToClient(client_index, "481 :Permission Denied- You're not an IRC operator\r\n");
		return;
	}
	std::string message = (params.size() > 2) ? params[2] : "";
	std::map<std::string, std::vector<Client*> >::iterator it = channels.find(channel);
	if (it == channels.end())
	{
		sendToClient(client_index, "403 " + channel + " :No such channel\r\n");
		return;
	}
	bool found = false;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (it->second[i]->getNickname() == target_nick)
		{
			std::string kick_message = ":" + clients[client_index]->getNickname() + " KICK " + channel + " " + target_nick + " :" + message + "\r\n";
			for (size_t j = 0; j < it->second.size(); ++j)
				sendToClient(std::distance(clients.begin(), std::find(clients.begin(), clients.end(), it->second[j])), kick_message);
			it->second.erase(it->second.begin() + i);
			found = true;
			break;
		}
	}
	if (!found)
		sendToClient(client_index, "441 " + target_nick + " " + channel + " :They aren't on that channel\r\n");
}

//----------------------INVITE-----------------------------------------

void ServerSocket::commandInvite(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing INVITE command" << std::endl;
	if (params.size() < 2)
	{
		sendToClient(client_index, "461 INVITE :Not enough parameters\r\n");
		return;
	}
	std::string target_nick = params[0];
	std::string channel = params[1];
	if (!isClientAutorize(channel_operators[channel], clients[client_index]))
	{
		sendToClient(client_index, "482 " + channel + " :You're not channel operator\r\n");
		sendToClient(client_index, "481 :Permission Denied- You're not an IRC operator\r\n");
		return;
	}
	bool found = false;
	for (std::vector<Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
	{
		if ((*it)->getNickname() == target_nick)
		{
			sendToClient(std::distance(clients.begin(), it), ":" + clients[client_index]->getNickname() + " INVITE " + target_nick + " :" + channel + "\r\n");
			sendToClient(client_index, "341 " + clients[client_index]->getNickname() + " " + target_nick + " " + channel + "\r\n");
			channel_invitations[channel].push_back(target_nick); // Ajout de l'invitation
			found = true;
			break;
		}
	}
	if (!found)
	{
		sendToClient(client_index, "401 " + target_nick + " :No such nick\r\n");
	}
}


//----------------------TOPIC-----------------------------------------

void ServerSocket::commandTopic(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing TOPIC command" << std::endl;
	if (params.size() < 1)
	{
		sendToClient(client_index, "461 TOPIC :Not enough parameters\r\n");
		return;
	}

	std::string channel = params[0];
	std::map<std::string, std::vector<Client*> >::iterator channel_it = channels.find(channel);
	if (channel_it == channels.end())
	{
		sendToClient(client_index, "403 " + channel + " :No such channel\r\n");
		return;
	}

	// Vérifiez si le mode +t est activé et si l'utilisateur est opérateur
	if (std::find(channel_modes[channel].begin(), channel_modes[channel].end(), 't') != channel_modes[channel].end() &&
		std::find(channel_operators[channel].begin(), channel_operators[channel].end(), clients[client_index]) == channel_operators[channel].end())
		{
		if (params.size() > 1)
		{ // L'utilisateur tente de modifier le sujet
			sendToClient(client_index, "482 " + channel + " :You're not channel operator\r\n");
			return;
		}
	}

	if (params.size() == 1)
	{
		// Afficher le sujet actuel du canal
		std::map<std::string, std::string>::iterator topic_it = topics.find(channel);
		if (topic_it != topics.end())
		{
			time_t topic_time = topic_times[channel];
			char time_str[32];
			std::strftime(time_str, sizeof(time_str), "%a %b %d %T %Y", std::localtime(&topic_time));
			sendToClient(client_index, "332 " + clients[client_index]->getNickname() + " " + channel + " :" + topic_it->second + "\r\n");
			sendToClient(client_index, "333 " + clients[client_index]->getNickname() + " " + channel + " " + clients[client_index]->getNickname() + " " + std::string(time_str) + "\r\n");
		}
		else
		{
			sendToClient(client_index, "331 " + clients[client_index]->getNickname() + " " + channel + " :No topic is set\r\n");
		}
	}
	else if (params.size() == 2 && params[0] == "-delete")
	{
		// Supprimer le sujet du canal
		topics.erase(channel);
		topic_set_by.erase(channel);
		sendToClient(client_index, "331 " + clients[client_index]->getNickname() + " " + channel + " :No topic is set\r\n");
	}
	else
	{
		// Construire le sujet du canal en traitant correctement les paramètres
		std::string topic;
		if (params[1][0] == ':')
		{
			topic = params[1].substr(1);
		}
		else
		{
			topic = params[1];
		}

		for (size_t i = 2; i < params.size(); ++i)
		{
			topic += " " + params[i];
		}
		topics[channel] = topic;

		// Définir la date de modification du topic
		time_t now = time(NULL);
		topic_times[channel] = now;
		topic_set_by[channel] = clients[client_index]->getNickname();

		std::string topicMessage = ":" + clients[client_index]->getNickname() + " TOPIC " + channel + " :" + topic + "\r\n";

		// Notifier tous les clients du canal du nouveau sujet une seule fois
		for (std::vector<Client*>::iterator it = channels[channel].begin(); it != channels[channel].end(); ++it)
		{
			sendToClient(std::distance(clients.begin(), std::find(clients.begin(), clients.end(), *it)), topicMessage);
		}
	}
}

//----------------------QUIT------------------------------------------

void ServerSocket::commandQuit(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing QUIT command" << std::endl;
	std::string message = "Client has quit";
	if (!params.empty())
	{
		message = params[0];
		for (size_t i = 1; i < params.size(); ++i)
		{
			message += " " + params[i];
		}
	}
	std::string quitMessage = ":" + clients[client_index]->getNickname() + " QUIT :" + message + "\r\n";
	for (size_t i = 0; i < clients.size(); ++i)
	{
		if (static_cast<int>(i) != client_index)
		{
			sendToClient(i, quitMessage);
		}
	}
	removeClient(client_index);
}


//----------------------MODE-----------------------------------------


int stringToInt(const std::string& str)
{
	std::istringstream iss(str);
	int num;
	iss >> num;
	return num;
}

template<typename T>
void vectorInsert(std::vector<T>& vec, const T& element)
{
	if (std::find(vec.begin(), vec.end(), element) == vec.end())
		vec.push_back(element);
}

template<typename T>
void vectorErase(std::vector<T>& vec, const T& element)
{
	typename std::vector<T>::iterator it = std::find(vec.begin(), vec.end(), element);
	if (it != vec.end())
		vec.erase(it);
}

bool	isClientAutorize(std::vector<Client*> vec, Client *user)
{
	bool	value = false;
	for (std::vector<Client*>::iterator it = vec.begin(); it != vec.end(); ++it)
	{
		if (*it == user)
			value = true;
	}
	return value;
}

void ServerSocket::commandMode(int client_index, const std::vector<std::string>& params)
{
	std::cerr << "Processing MODE command" << std::endl;
	if (params.size() < 2)
	{
		sendToClient(client_index, "461 MODE :Not enough parameters\r\n");
		return;
	}
	std::string channel = params[0];
	std::string modes = params[1];
	bool add_mode = true;
	std::map<std::string, std::vector<Client*> >::iterator it = channels.find(channel);
	if (it == channels.end())
	{
		sendToClient(client_index, "403 " + channel + " :No such channel\r\n");
		return;
	}
	if (!isClientAutorize(channel_operators[channel], clients[client_index]))
	{
		sendToClient(client_index, "481 :Permission Denied- You're not an IRC operator\r\n");
		sendToClient(client_index, "482 " + channel + " :You're not channel operator\r\n");
		return;
	}
	Client* target = NULL; // Déclaration ici pour éviter l'erreur de saut
	for (size_t i = 0; i < modes.size(); ++i)
	{
		char mode = modes[i];
		switch (mode) {
			case '+':
				add_mode = true;
				break;
			case '-':
				add_mode = false;
				break;
			case 'i':
				if (add_mode)
				{
					vectorInsert(channel_modes[channel], 'i');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " +i\r\n");
				}
				else
				{
					vectorErase(channel_modes[channel], 'i');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " -i\r\n");
				}
				break;
			case 'k':
				if (add_mode) {
					if (params.size() < 3) {
						sendToClient(client_index, "461 MODE :Not enough parameters for +k\r\n");
						return;
					}
					// Ajouter un mot de passe au canal
					channel_passwords[channel] = params[2];
					channel_modes[channel].push_back('k');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " +k " + params[2] + "\r\n");
				} else {
					// Supprimer le mot de passe du canal
					channel_passwords.erase(channel);
					channel_modes[channel].erase(std::remove(channel_modes[channel].begin(), channel_modes[channel].end(), 'k'), channel_modes[channel].end());
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " -k\r\n");
				}
				break;
			case 'l':
				if (add_mode) {
					if (params.size() < 3) {
						sendToClient(client_index, "461 MODE :Not enough parameters for +l\r\n");
						return;
					}
					// Limiter le nombre d'utilisateurs dans le canal
					channel_limits[channel] = std::atoi(params[2].c_str());
					channel_modes[channel].push_back('l');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " +l " + params[2] + "\r\n");
				} else {
					// Supprimer la limite du nombre d'utilisateurs
					channel_limits.erase(channel);
					channel_modes[channel].erase(std::remove(channel_modes[channel].begin(), channel_modes[channel].end(), 'l'), channel_modes[channel].end());
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " -l\r\n");
				}
				break;
			case 't':
				if (add_mode)
				{
					vectorInsert(channel_modes[channel], 't');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " +t\r\n");
				}
				else
				{
					vectorErase(channel_modes[channel], 't');
					sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " -t\r\n");
				}
				break;
			case 'o':
				if (params.size() < 3)
				{
					sendToClient(client_index, "461 MODE :Not enough parameters for +o\r\n");
					return;
				}
				target = NULL;
				for (size_t j = 0; j < clients.size(); ++j)
				{
					if (clients[j]->getNickname() == params[2])
					{
						target = clients[j];
						break;
					}
				}
				if (target)
				{
					if (add_mode)
					{
						vectorInsert(channel_operators[channel], target);
						sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " +o " + params[2] + "\r\n");
					}
					else
					{
						vectorErase(channel_operators[channel], target);
						sendToClient(client_index, ":" + clients[client_index]->getNickname() + " MODE " + channel + " -o " + params[2] + "\r\n");
					}
				}
				else
					sendToClient(client_index, "401 " + params[2] + " :No such nick/channel\r\n");
				break;
			default:
				sendToClient(client_index, "472 " + channel + " " + mode + " :is unknown mode char to me\r\n");
				break;
		}
	}
}
