#include <gtest/gtest.h>
#include <atomic>
#include <thread>

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
            ++callCount;
            requestStarted = true;
            while (blockRequest)
                std::this_thread::yield();
            return (this->_result);
        }
        bool called = false;
        std::atomic<int> callCount{0};
        std::atomic<bool> blockRequest{false};
        std::atomic<bool> requestStarted{false};
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
        auth::JwksResult::Status::Success
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
        auth::JwksResult::Status::Success
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
        auth::JwksResult::Status::KeyNotFound
    );

    EXPECT_FALSE(result.key.has_value());
}

TEST(HttpJwksProviderTest, ReturnsUnavailableWhenHttpRequestFails)
{
    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    auth::JsonJwksParser parser;

    auth::HttpResult httpResult{
        auth::HttpResult::Status::NetworkError,
        std::nullopt
    };

    FakeHttpClient httpClient(httpResult);

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
        auth::JwksResult::Status::Unavailable
    );

    EXPECT_FALSE(result.key.has_value());
}

TEST(HttpJwksProviderTest, ReturnsUnavailableWhenHttpResponseIsNotSuccessful)
{
    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    auth::JsonJwksParser parser;

    auth::HttpResponse response{
        500,
        "Internal Server Error"
    };

    auth::HttpResult httpResult{
        auth::HttpResult::Status::Success,
        response
    };

    FakeHttpClient httpClient(httpResult);

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
        auth::JwksResult::Status::Unavailable
    );

    EXPECT_FALSE(result.key.has_value());
}

TEST(HttpJwksProviderTest, ReturnsUnavailableWhenJwksResponseIsInvalid)
{
    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    auth::JsonJwksParser parser;

    auth::HttpResponse response{
        200,
        R"({
            "this_is_not": "a valid JWKS"
        })"
    };

    auth::HttpResult httpResult{
        auth::HttpResult::Status::Success,
        response
    };

    FakeHttpClient httpClient(httpResult);

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
        auth::JwksResult::Status::Unavailable
    );

    EXPECT_FALSE(result.key.has_value());
}

TEST(HttpJwksProviderTest, RefreshReplacesOldKeys)
{
    auth::Jwk oldKey{
        "key-old",
        "RSA",
        "RS256",
        "old-modulus",
        "AQAB"
    };

    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    cache.replace({oldKey});

    clock.advance(std::chrono::seconds(61));

    auth::HttpResponse response{
        200,
        R"({
            "keys": [
                {
                    "kid": "key-new",
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

    const auto result = provider.getKey("key-new");

    EXPECT_TRUE(httpClient.called);

    EXPECT_EQ(
        result.status,
        auth::JwksResult::Status::Success
    );

    ASSERT_TRUE(result.key.has_value());

    EXPECT_EQ(
        result.key->kid,
        "key-new"
    );

    const auto oldKeyResult = cache.get("key-old");

    EXPECT_EQ(
        oldKeyResult.status,
        auth::JwksCacheResult::Status::NotFound
    );

    const auto oldKeyResult2 = cache.get("key-123");

    EXPECT_EQ(
        oldKeyResult2.status,
        auth::JwksCacheResult::Status::NotFound
    );
}

TEST(HttpJwksProviderTest, RefreshesOnlyOnceWhenMultipleThreadsRequestSameKey)
{
    auth::Jwk oldKey{
        "key-123",
        "RSA",
        "RS256",
        "old-modulus",
        "AQAB"
    };

    FakeClock clock;
    auth::JwksCache cache(std::chrono::seconds(60), clock);
    cache.replace({oldKey});

    clock.advance(std::chrono::seconds(61));

    auth::HttpResponse response{
        200,
        R"({
            "keys": [
                {
                    "kid": "key-123",
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
    httpClient.blockRequest = true;

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

    auth::JwksResult resultA;
    auth::JwksResult resultB;

    std::thread threadA([&]()
    {
        resultA = provider.getKey("key-123");
    });

    while (!httpClient.requestStarted)
        std::this_thread::yield();

    std::thread threadB([&]()
    {
        resultB = provider.getKey("key-123");
    });

    std::this_thread::yield();

    httpClient.blockRequest = false;

    threadA.join();
    threadB.join();

    EXPECT_EQ(httpClient.callCount, 1);

    EXPECT_EQ(
        resultA.status,
        auth::JwksResult::Status::Success
    );

    EXPECT_EQ(
        resultB.status,
        auth::JwksResult::Status::Success
    );

    ASSERT_TRUE(resultA.key.has_value());
    ASSERT_TRUE(resultB.key.has_value());

    EXPECT_EQ(resultA.key->kid, "key-123");
    EXPECT_EQ(resultB.key->kid, "key-123");
}