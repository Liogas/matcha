#include <auth/jwks/HttpJwksProvider.hpp>
#include <utility>

namespace auth
{
	HttpJwksProvider::HttpJwksProvider(
		HttpJwksConfig config,
		HttpClient &httpClient,
		JwksParser &parser,
		JwksCache &cache
	):
		_config(std::move(config)),
		_httpClient(httpClient),
		_parser(parser),
		_cache(cache)
	{}

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
		if (cacheResult.status == JwksCacheResult::Status::NotFound)
			return {
				JwksResult::Status::KeyNotFound,
				std::nullopt
			};
		std::lock_guard<std::mutex> lock(this->_refreshMutex);
		const auto refreshedCacheResult = this->_cache.get(kid);
		if (refreshedCacheResult.status == JwksCacheResult::Status::Found)
			return {
				JwksResult::Status::Success,
				refreshedCacheResult.key
			};
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