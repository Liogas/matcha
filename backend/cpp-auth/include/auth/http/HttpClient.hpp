#pragma once

#include "HttpResult.hpp"

namespace	auth
{
	class HttpClient
	{
		public:
			virtual ~HttpClient() = default;
			virtual HttpResult get(
				const std::string &url
			) = 0;
	};
}