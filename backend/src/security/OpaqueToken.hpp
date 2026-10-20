#pragma once

#include <string>

struct OpaqueToken
{
	std::string value;
	std::string hash;
};

OpaqueToken generateOpaqueToken();
std::string hashOpaqueToken(const std::string &token);
