#pragma once

#include <sw/redis++/redis++.h>

#include <chrono>
#include <string>

struct LoginAttemptDecision
{
	bool allowed;
	int retryAfterSeconds;
};

class RedisLoginAttemptLimiter
{
	public:
		RedisLoginAttemptLimiter(
			sw::redis::Redis &redis,
			unsigned int maxAttempts,
			std::chrono::seconds window
		);

		LoginAttemptDecision consume(
			const std::string &email,
			const std::string &clientIp
		);

		void clear(
			const std::string &email,
			const std::string &clientIp
		);

	private:
		static std::string keyFor(
			const std::string &email,
			const std::string &clientIp
		);

		sw::redis::Redis &_redis;
		unsigned int _maxAttempts;
		std::chrono::seconds _window;
};
