#include <gtest/gtest.h>
#include <auth/jwks/JwksCache.hpp>

TEST(JwksCacheTest, FindsExistingKey)
{
	auth::Jwk key{
		.kid = "key-1",
		.kty = "RSA",
		.alg = "RS256",
		.n = "modulus",
		.e = "AQAB"
	};

	auth::JwksCache cache(std::chrono::seconds(300));
	cache.replace({key});
	const auto result = cache.get("key-1");
	EXPECT_EQ(
		result.status,
		auth::JwksCacheResult::Status::Found
	);
	ASSERT_TRUE(result.key.has_value());
	EXPECT_EQ(result.key->kid, "key-1");
}

TEST(JwksCacheTest, ReturnsNotFoundForUnknownKey)
{
    auth::JwksCache cache(std::chrono::seconds(300));

    auth::Jwk key{
        .kid = "key-1",
        .kty = "RSA",
        .alg = "RS256",
        .n = "modulus",
        .e = "AQAB"
    };

    cache.replace({key});

    const auto result = cache.get("unknown");

    EXPECT_EQ(
        result.status,
        auth::JwksCacheResult::Status::NotFound
    );

    EXPECT_FALSE(result.key.has_value());
}

TEST(JwksCacheTest, ReturnsExpiredWhenCacheIsExpired)
{
    auth::JwksCache cache(std::chrono::seconds(0));

    auth::Jwk key{
        .kid = "key-1",
        .kty = "RSA",
        .alg = "RS256",
        .n = "modulus",
        .e = "AQAB"
    };

    cache.replace({key});

    const auto result = cache.get("key-1");

    EXPECT_EQ(
        result.status,
        auth::JwksCacheResult::Status::Expired
    );

    EXPECT_FALSE(result.key.has_value());
}