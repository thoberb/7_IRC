/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bberthod <bberthod@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/20 16:20:50 by bberthod          #+#    #+#             */
/*   Updated: 2024/07/15 18:55:09 by bberthod         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include <unistd.h> // close

Client::Client(int fd, const std::string& address) : fd(fd), address(address), hostname("localhost"), is_nick_set(false), is_user_set(false), registered(false), authenticated(false){}

int Client::getFd() const
{
	return fd;
}

std::string Client::getAddress() const
{
	return address;
}

bool	Client::operator==(const Client &A) const
{
	return (address == A.getAddress() && nickname == A.getNickname() && username == A.getUsername() && \
		hostname == A.getHostname() && realname == A.getRealname() && is_nick_set == A.isNickSet() && \
		is_user_set == A.isUserSet() && registered == A.isFullyRegistered());
}

std::string Client::getNickname() const
{
	return nickname;
}

void Client::setNickname(const std::string& nickname)
{
	this->nickname = nickname;
	is_nick_set = true;
}

std::string Client::getUsername() const
{
	return username;
}

void Client::setUsername(const std::string& username)
{
	this->username = username;
	is_user_set = true;
}

std::string Client::getRealname() const
{
	return realname;
}

void Client::setRealname(const std::string& realname)
{
	this->realname = realname;
}

std::string	Client::getBuffer() const
{
	return buffer;
}

void	Client::addToBuffer(std::string const& line)
{
	buffer.append(line);
}

void	Client::clearBuffer()
{
	buffer.clear();
}

bool Client::isFullyRegistered() const
{
	return is_nick_set && is_user_set && registered;
}

bool Client::isNickSet() const
{
	return is_nick_set;
}

bool Client::isUserSet() const
{
	return is_user_set;
}

void Client::setNickSet(bool value)
{
	is_nick_set = value;
}

void Client::setUserSet(bool value)
{
	is_user_set = value;
}

void Client::setRegistered(bool set)
{
	registered = set;
}

void Client::setAuthenticated(bool auth)
{
	authenticated = auth;
}

bool Client::isAuthenticated() const
{
	return authenticated;
}
