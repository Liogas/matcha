#include <gtest/gtest.h>

#include <auth/jwks/HttpJwksProvider.hpp>
#include <auth/jwks/JsonJwksParser.hpp>
#include <auth/jwks/JwksCache.hpp>

#include "FakeClock.hpp"

class FakeHttpClient : public auth::HttpClient
{
    public:
        explicit FakeHttpClient(auth::HttpResult result)
            : _result(std::move(result))
        {}
        auth::HttpResult get(const std::string&) override
        {
            called = true;
            return (this->_result);
        }
        bool called = false;
    private:
        auth::HttpResult    _result;
};

TEST(HttpJwksProviderTest, ReturnsCachedKey)
{
    auth::Jwk key{
        "key-123",
        "RSA",
        "RS256",
        "modulus",
        "AQAB"
    };
    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    cache.replace({key});

    auth::HttpResult httpResult{
        auth::HttpResult::Status::NetworkError,
        std::nullopt
    };
    FakeHttpClient httpClient(httpResult);
    auth::JsonJwksParser parser;

    auth::HttpJwksConfig config{
        "https://example.com/.well-known/jwks.json",
        std::chrono::seconds(60)
    };

    auth::HttpJwksProvider provider(
        config,
        httpClient,
        parser,
        cache
    );

    const auto result = provider.getKey("key-123");
    EXPECT_EQ(
        result.status,
        JwksResult::Status::Success
    );
    ASSERT_TRUE(result.key.has_value());
    EXPECT_EQ(
        result.key->kid,
        "key-123"
    );
    EXPECT_FALSE(httpClient.called);
}

TEST(HttpJwksProviderTest, RefreshesExpiredCache)
{
    auth::Jwk key{
        "key-123",
        "RSA",
        "RS256",
        "modulus",
        "AQAB"
    };
	FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    cache.replace({key});
	clock.advance(std::chrono::seconds(61));
	const auto cacheResult = cache.get("key-123");

	EXPECT_EQ(
    	cacheResult.status,
    	auth::JwksCacheResult::Status::Expired
	);	

    auth::HttpResponse response{
        200,
        R"({
            "keys": [
                {
                    "kid": "key-123",
                    "kty": "RSA",
                    "alg": "RS256",
                    "n": "modulus",
                    "e": "AQAB"
                }
            ]
        })"
    };

    auth::HttpResult httpResult{
        auth::HttpResult::Status::Success,
        response
    };

    FakeHttpClient httpClient(httpResult);
    auth::JsonJwksParser parser;

    auth::HttpJwksConfig config{
        "https://example.com/.well-known/jwks.json",
        std::chrono::seconds(60)
    };

    auth::HttpJwksProvider provider(
        config,
        httpClient,
        parser,
        cache
    );

    const auto result = provider.getKey("key-123");

    EXPECT_TRUE(httpClient.called);
    EXPECT_EQ(
        result.status,
        JwksResult::Status::Success
    );
    ASSERT_TRUE(result.key.has_value());
    EXPECT_EQ(
        result.key->kid,
        "key-123"
    );
}

TEST(HttpJwksProviderTest, ReturnsKeyNotFoundWhenRefreshedJwksDoesNotContainKid)
{
    auth::Jwk oldKey{
        "key-123",
        "RSA",
        "RS256",
        "old-modulus",
        "AQAB"
    };
	FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(0), clock);
    cache.replace({oldKey});

    auth::HttpResponse response{
        200,
        R"({
            "keys": [
                {
                    "kid": "key-456",
                    "kty": "RSA",
                    "alg": "RS256",
                    "n": "new-modulus",
                    "e": "AQAB"
                }
            ]
        })"
    };

    auth::HttpResult httpResult{
        auth::HttpResult::Status::Success,
        response
    };

    FakeHttpClient httpClient(httpResult);
    auth::JsonJwksParser parser;

    auth::HttpJwksConfig config{
        "https://example.com/.well-known/jwks.json",
        std::chrono::seconds(60)
    };

    auth::HttpJwksProvider provider(
        config,
        httpClient,
        parser,
        cache
    );

    const auto result = provider.getKey("key-123");

    EXPECT_TRUE(httpClient.called);

    EXPECT_EQ(
        result.status,
        JwksResult::Status::KeyNotFound
    );

    EXPECT_FALSE(result.key.has_value());
}