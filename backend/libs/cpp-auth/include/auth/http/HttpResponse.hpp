#pragma once

#include <string>

namespace	auth
{
	struct	HttpResponse
	{
		int			statusCode;
		std::string	body;
	};
}