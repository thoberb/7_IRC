/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bberthod <bberthod@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/20 16:09:36 by bberthod          #+#    #+#             */
/*   Updated: 2024/07/15 18:55:31 by bberthod         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <netinet/in.h>

class Client
{
	private:
		int fd;
		std::string address;
		std::string nickname;
		std::string username;
		std::string hostname;
		std::string realname;
		std::string buffer;
		bool is_nick_set;
		bool is_user_set;
		bool registered;
		bool authenticated;

	public:
		Client(int fd, const std::string& address);
		bool	operator==(const Client &A) const;
		int getFd() const;
		std::string getAddress() const;
		std::string getNickname() const;
		void setNickname(const std::string& nickname);
		std::string getUsername() const;
		void setUsername(const std::string& username);
		std::string getRealname() const;
		void setRealname(const std::string& realname);
		void setHostname(const std::string& host) { hostname = host; }
		std::string getHostname() const { return hostname; }
		std::string	getBuffer() const;
		void	addToBuffer(std::string const& line);
		void	clearBuffer();
		bool isFullyRegistered() const;
		bool isNickSet() const;
		bool isUserSet() const;
		void setNickSet(bool value);
		void setUserSet(bool value);
		void setRegistered(bool set);
		void setAuthenticated(bool auth);
		bool isAuthenticated() const;
};

#endif

