#include <gtest/gtest.h>
#include <sw/redis++/redis++.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "../../src/security/RedisLoginAttemptLimiter.hpp"

namespace
{
	std::string requiredEnvironmentVariable(const char *name)
	{
		const char *value = std::getenv(name);
		if (value == nullptr || *value == '\0')
			throw std::runtime_error(
				std::string("Missing test environment variable: ") + name
			);
		return value;
	}

	std::string readRedisPassword()
	{
		std::ifstream file(
			requiredEnvironmentVariable("REDIS_PASSWORD_FILE"),
			std::ios::binary
		);
		if (!file)
			throw std::runtime_error(
				"Unable to read Redis password secret for tests"
			);

		std::string password{
			std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>()
		};
		while (!password.empty() &&
			(password.back() == '\n' || password.back() == '\r'))
			password.pop_back();
		if (password.empty())
			throw std::runtime_error("Redis password secret is empty");
		return password;
	}

	sw::redis::Redis makeRedisClient()
	{
		sw::redis::ConnectionOptions connectionOptions;
		connectionOptions.host =
			requiredEnvironmentVariable("REDIS_HOST");
		connectionOptions.port = std::stoi(
			requiredEnvironmentVariable("REDIS_PORT")
		);
		connectionOptions.password = readRedisPassword();
		connectionOptions.connect_timeout = std::chrono::seconds{1};
		connectionOptions.socket_timeout = std::chrono::seconds{1};

		sw::redis::ConnectionPoolOptions poolOptions;
		poolOptions.size = 2;
		poolOptions.wait_timeout = std::chrono::seconds{1};
		return sw::redis::Redis(connectionOptions, poolOptions);
	}
}

TEST(RedisLoginAttemptLimiterTest, LimitsEachAccountAndIpPair)
{
	auto redis = makeRedisClient();
	RedisLoginAttemptLimiter limiter(redis, 2, std::chrono::seconds{30});
	const auto email =
		"limit-test-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		) + "@example.com";
	const auto otherEmail = "other-" + email;

	EXPECT_TRUE(limiter.consume(email, "192.0.2.1").allowed);
	EXPECT_TRUE(limiter.consume(email, "192.0.2.1").allowed);

	const auto blocked = limiter.consume(email, "192.0.2.1");
	EXPECT_FALSE(blocked.allowed);
	EXPECT_GT(blocked.retryAfterSeconds, 0);

	EXPECT_TRUE(limiter.consume(email, "192.0.2.2").allowed);
	EXPECT_TRUE(limiter.consume(otherEmail, "192.0.2.1").allowed);

	limiter.clear(email, "192.0.2.1");
	EXPECT_TRUE(limiter.consume(email, "192.0.2.1").allowed);
}

TEST(RedisLoginAttemptLimiterTest, ExpiresTheCounterAfterItsWindow)
{
	auto redis = makeRedisClient();
	RedisLoginAttemptLimiter limiter(redis, 1, std::chrono::seconds{1});
	const auto email =
		"expiry-test-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		) + "@example.com";

	EXPECT_TRUE(limiter.consume(email, "192.0.2.10").allowed);
	EXPECT_FALSE(limiter.consume(email, "192.0.2.10").allowed);

	std::this_thread::sleep_for(std::chrono::milliseconds{1200});
	EXPECT_TRUE(limiter.consume(email, "192.0.2.10").allowed);
}
