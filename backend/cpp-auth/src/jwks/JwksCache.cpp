#include <auth/jwks/JwksCache.hpp>

namespace	auth
{
	JwksCache::JwksCache(std::chrono::seconds ttl):
		_ttl(ttl),
		_fetchedAt(std::chrono::steady_clock::now())
	{
	}

	void	JwksCache::replace(const std::vector<Jwk> &keys)
	{
		std::lock_guard<std::mutex> lock(this->_mutex);
		this->_keys.clear();
		for (const auto &key : keys)
			this->_keys[key.kid] = key;
		this->_fetchedAt = std::chrono::steady_clock::now();
	}

	bool	JwksCache::isExpired() const
	{
		if (!this->_fetchedAt.has_value())
			return (true);
		const auto now = std::chrono::steady_clock::now();
		const auto age = now - *this->_fetchedAt;
		return age >= this->_ttl;
	}

	JwksCacheResult	JwksCache::get(const std::string &kid) const
	{
		std::lock_guard<std::mutex> lock(this->_mutex);
		if (this->isExpired())
			return {
				JwksCacheResult::Status::Expired,
				std::nullopt
			};
		const auto it = this->_keys.find(kid);
		if (it == this->_keys.end())
			return {
				JwksCacheResult::Status::NotFound,
				std::nullopt
			};
		return {
			JwksCacheResult::Status::Found,
			it->second
		};
	}
}