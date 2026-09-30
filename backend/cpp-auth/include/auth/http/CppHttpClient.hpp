#pragma once

#include <string>
#include <optional>

#include <auth/http/HttpClient.hpp>
#include <auth/http/HttpResult.hpp>

namespace auth
{
	class CppHttpClient : public HttpClient
	{
		public:
			HttpResult get(const std::string &url) override;
	};
}