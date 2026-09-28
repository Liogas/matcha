#pragma once

#include <string>

namespace	auth
{
	struct JWK
	{
		std::string kid;
		std::string kty;
		std::string alg;
		std::string n;
		std::string e;
	};
}