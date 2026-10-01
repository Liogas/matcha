#pragma once

#include <string>
#include <optional>

#include <auth/http/HttpClient.hpp>
#include <auth/http/HttpResult.hpp>
#include <auth/http/HttpClientConfig.hpp>

namespace auth
{
	class CppHttpClient : public HttpClient
	{
		public:
			explicit CppHttpClient(
				const HttpClientConfig &config
			);
			HttpResult get(const std::string &url) override;
		private:
			HttpClientConfig	_config;
	};
}