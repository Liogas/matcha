#pragma once

#include <string>

struct	TokenValidatorConfig
{
	std::string issuer;
	std::string audience;
	std::string algorithm;
};