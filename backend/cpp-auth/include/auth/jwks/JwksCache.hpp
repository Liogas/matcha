#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <auth/time/IClock.hpp>
#include "JwksCacheResult.hpp"

namespace	auth
{
	class JwksCache
	{
		public:
			explicit JwksCache(std::chrono::seconds ttl, IClock &clock);
			JwksCacheResult	get(const std::string &kid) const;
			void			replace(const std::vector<Jwk> &key);
		private:
			bool	isExpired() const;
			std::chrono::seconds					_ttl;
			std::unordered_map<std::string, Jwk>	_keys;
			std::optional<std::chrono::steady_clock::time_point>
													_fetchedAt;
			mutable	std::mutex						_mutex;
			IClock									&_clock;
	};
}