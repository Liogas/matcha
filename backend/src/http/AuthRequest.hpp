#pragma once

#include <string>

struct RegisterRequest
{
	std::string email;
	std::string password;
};

struct LoginRequest
{
	std::string email;
	std::string password;
};