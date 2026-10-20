#pragma once

#include <string>

struct RegisterRequest
{
	std::string email;
	std::string username;
	std::string firstName;
	std::string lastName;
	std::string password;
};

struct LoginRequest
{
	std::string email;
	std::string password;
};