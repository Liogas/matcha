#include "RedisLoginAttemptLimiter.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
	constexpr char ATTEMPT_SCRIPT[] = R"lua(
local count = redis.call('INCR', KEYS[1])
if count == 1 then
    redis.call('EXPIRE', KEYS[1], ARGV[1])
end
if count > tonumber(ARGV[2]) then
    return {0, redis.call('TTL', KEYS[1])}
end
return {1, 0}
)lua";
}

RedisLoginAttemptLimiter::RedisLoginAttemptLimiter(
	sw::redis::Redis &redis,
	unsigned int maxAttempts,
	std::chrono::seconds window
):
	_redis(redis),
	_maxAttempts(maxAttempts),
	_window(window)
{
	if (this->_maxAttempts == 0 ||
		this->_window <= std::chrono::seconds::zero())
		throw std::invalid_argument(
			"Login attempt limit and window must be positive"
		);
}

LoginAttemptDecision RedisLoginAttemptLimiter::consume(
	const std::string &email,
	const std::string &clientIp
)
{
	if (email.empty() || clientIp.empty())
		throw std::invalid_argument(
			"Login account and client IP cannot be empty"
		);

	const auto key = keyFor(email, clientIp);
	const auto result = this->_redis.eval<std::vector<long long>>(
		ATTEMPT_SCRIPT,
		{key},
		{
			std::to_string(this->_window.count()),
			std::to_string(this->_maxAttempts)
		}
	);
	if (result.size() != 2)
		throw std::runtime_error(
			"Redis returned an invalid login attempt result"
		);

	if (result[0] == 1)
		return {true, 0};
	if (result[0] != 0)
		throw std::runtime_error(
			"Redis returned an unknown login attempt status"
		);

	return {
		false,
		static_cast<int>(std::max(1LL, result[1]))
	};
}

void RedisLoginAttemptLimiter::clear(
	const std::string &email,
	const std::string &clientIp
)
{
	if (email.empty() || clientIp.empty())
		throw std::invalid_argument(
			"Login account and client IP cannot be empty"
		);
	this->_redis.del(keyFor(email, clientIp));
}

std::string RedisLoginAttemptLimiter::keyFor(
	const std::string &email,
	const std::string &clientIp
)
{
	const std::string identity =
		std::to_string(email.size()) + ":" + email +
		std::to_string(clientIp.size()) + ":" + clientIp;

	unsigned char digest[SHA256_DIGEST_LENGTH];
	if (SHA256(
			reinterpret_cast<const unsigned char *>(identity.data()),
			identity.size(),
			digest
		) == nullptr)
		throw std::runtime_error(
			"Unable to hash login attempt identity"
		);

	std::ostringstream key;
	key << "matcha:login-attempt:v1:" << std::hex << std::setfill('0');
	for (const unsigned char byte : digest)
		key << std::setw(2) << static_cast<unsigned int>(byte);
	return key.str();
}
