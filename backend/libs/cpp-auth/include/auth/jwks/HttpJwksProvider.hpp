#pragma once

#include <optional>
#include <mutex>

#include <auth/jwks/JwksProvider.hpp>
#include <auth/jwks/HttpJwksConfig.hpp>
#include <auth/jwks/JwksParser.hpp>
#include <auth/jwks/JwksCache.hpp>
#include <auth/http/HttpClient.hpp>
#include <auth/time/IClock.hpp>

namespace auth
{
	class HttpJwksProvider : public JwksProvider
	{
		public:
			HttpJwksProvider(
				HttpJwksConfig config,
				HttpClient &httpClient,
				JwksParser &parser,
				JwksCache &cache,
				IClock &clock
			);
			JwksResult	getKey(
				const std::string &kid
			) override;
		private:
			HttpJwksConfig 	_config;
			HttpClient 		&_httpClient;
			JwksParser		&_parser;
			JwksCache		&_cache;
			IClock			&_clock;
			std::mutex		_refreshMutex;
			std::optional<std::chrono::steady_clock::time_point>
							_lastRefreshAttempt;
			bool			_lastRefreshSucceeded = false;
	};
}