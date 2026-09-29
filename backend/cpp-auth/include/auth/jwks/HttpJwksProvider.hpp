#pragma once

#include <mutex>

#include <auth/jwks/JwksProvider.hpp>
#include <auth/jwks/HttpJwksConfig.hpp>
#include <auth/jwks/JwksParser.hpp>
#include <auth/jwks/JwksCache.hpp>
#include <auth/http/HttpClient.hpp>

namespace auth
{
	class HttpJwksProvider : public JwksProvider
	{
		public:
			HttpJwksProvider(
				HttpJwksConfig config,
				HttpClient &httpClient,
				JwksParser &parser,
				JwksCache &cache
			);
			JwksResult	getKey(
				const std::string &kid
			) override;
		private:
			HttpJwksConfig 	_config;
			HttpClient 		&_httpClient;
			JwksParser		&_parser;
			JwksCache		&_cache;
			std::mutex		_refreshMutex;
	};
}