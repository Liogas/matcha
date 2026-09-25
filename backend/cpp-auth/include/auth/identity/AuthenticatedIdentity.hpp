#pragma once

#include <string>

struct AuthenticatedIdentity
{
	std::string subject;
	std::string issuer;
};