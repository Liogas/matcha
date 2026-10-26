#include <auth/jwks/HttpJwksProvider.hpp>

#include <stdexcept>
#include <utility>

namespace auth
{
	HttpJwksProvider::HttpJwksProvider(
		HttpJwksConfig config,
		HttpClient &httpClient,
		JwksParser &parser,
		JwksCache &cache,
		IClock &clock
	):
		_config(std::move(config)),
		_httpClient(httpClient),
		_parser(parser),
		_cache(cache),
		_clock(clock)
	{
		if (this->_config.jwksUrl.empty())
			throw std::invalid_argument(
				"JWKS URL cannot be empty"
			);
		if (this->_config.refreshCooldown <=
			std::chrono::seconds::zero())
			throw std::invalid_argument(
				"JWKS refresh cooldown must be positive"
			);
	}

	JwksResult	HttpJwksProvider::getKey(
		const std::string &kid
	)
	{
		const auto cacheResult = this->_cache.get(kid);
		if (cacheResult.status == JwksCacheResult::Status::Found)
			return {
				JwksResult::Status::Success,
				cacheResult.key
			};
		std::lock_guard<std::mutex> lock(this->_refreshMutex);
		const auto refreshedCacheResult = this->_cache.get(kid);
		if (refreshedCacheResult.status == JwksCacheResult::Status::Found)
			return {
				JwksResult::Status::Success,
				refreshedCacheResult.key
			};
		const auto now = this->_clock.now();
		if (this->_lastRefreshAttempt.has_value()
			&& now - *this->_lastRefreshAttempt
				< this->_config.refreshCooldown)
		{
			if (refreshedCacheResult.status
					== JwksCacheResult::Status::NotFound
				&& this->_lastRefreshSucceeded)
				return {
					JwksResult::Status::KeyNotFound,
					std::nullopt
				};
			return {
				JwksResult::Status::Unavailable,
				std::nullopt
			};
		}
		this->_lastRefreshAttempt = now;
		this->_lastRefreshSucceeded = false;
		const auto httpResult = this->_httpClient.get(
			this->_config.jwksUrl
		);
		if (httpResult.status == HttpResult::Status::NetworkError)
			return {
				JwksResult::Status::Unavailable,
				std::nullopt
			};
		if (!httpResult.response)
			return {
				JwksResult::Status::Unavailable,
				std::nullopt
			};
		if (httpResult.response->statusCode != 200)
			return {
				JwksResult::Status::Unavailable,
				std::nullopt
			};
		const auto parseResult = this->_parser.parse(
			httpResult.response->body
		);
		if (parseResult.status == JwksParseResult::Status::InvalidJwks)
			return {
				JwksResult::Status::Unavailable,
				std::nullopt
			};
		this->_cache.replace(parseResult.keys);
		this->_lastRefreshSucceeded = true;
		const auto finalCacheResult = this->_cache.get(kid);
		if (finalCacheResult.status == JwksCacheResult::Status::Found)
			return {
				JwksResult::Status::Success,
				finalCacheResult.key
			};
		return {
			JwksResult::Status::KeyNotFound,
			std::nullopt
		};
	}
}