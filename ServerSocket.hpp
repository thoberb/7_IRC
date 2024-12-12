/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bberthod <bberthod@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/09 11:57:49 by bberthod          #+#    #+#             */
/*   Updated: 2024/07/16 16:12:14 by bberthod         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include <netinet/in.h>
#include <vector>
#include <map>
#include "Client.hpp"
#include <ctime>

class ServerSocket
{
	public:
		ServerSocket(const std::string& password);
		~ServerSocket();
		bool setup(int port);
		static void closeServer(int signal);
		int acceptConnection();
		int getSocket() const;
		void handleClient(int index);
		void removeClient(int index);
		void sendToClient(int index, const std::string& message);

		void handleCommand(int client_index, const std::string& command);
		void commandPass(int client_index, const std::vector<std::string>& params);
		void commandNick(int client_index, const std::vector<std::string>& params);
		void commandUser(int client_index, const std::vector<std::string>& params);
		void commandJoin(int client_index, const std::vector<std::string>& params);
		void commandPrivmsg(int client_index, const std::vector<std::string>& params);
		void commandKick(int client_index, const std::vector<std::string>& params);
		void commandInvite(int client_index, const std::vector<std::string>& params);
		void commandTopic(int client_index, const std::vector<std::string>& params);
		void commandQuit(int client_index, const std::vector<std::string>& params);
		void commandMode(int client_index, const std::vector<std::string>& params);
		void run();

		std::string generateUniqueNickname(const std::string& base_nickname);
		static bool nicknameMatches(Client* client, const std::string& nickname);
		Client* findClientByNickname(const std::string& nickname);
		std::vector<struct pollfd> poll_fds;  // Liste des descripteurs de fichiers surveillés

	private:
		std::string server_password;
		int server_socket;
		static ServerSocket *_ptrServer;
		struct sockaddr_in server_addr;
		std::vector<Client*> clients;
		std::map<std::string, std::vector<Client*> > channels;
		std::map<std::string, std::string> topics;
		std::map<std::string, time_t> topic_times;
		std::map<std::string, std::string> topic_set_by; // Nom de l'utilisateur qui a défini le topic
		std::map<std::string, std::vector<char> > channel_modes; // Modes de canal
		std::map<std::string, std::string> channel_passwords; // Mots de passe de canal
		std::map<std::string, int> channel_limits; // Limites de canal
		std::map<std::string, std::vector<Client*> > channel_operators; // Opérateurs de canal
		std::map<std::string, std::vector<std::string> > channel_invitations; // Invitations de canal
		std::map<std::string, std::vector<Client*> > pending_invites; //tentatives de connexion


};

bool	isClientAutorize(std::vector<Client*> vec, Client *user);

#endif
