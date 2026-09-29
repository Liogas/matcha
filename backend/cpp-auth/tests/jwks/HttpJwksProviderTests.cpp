#include <gtest/gtest.h>

#include <auth/jwks/HttpJwksProvider.hpp>
#include <auth/jwks/JsonJwksParser.hpp>
#include <auth/jwks/JwksCache.hpp>

class FakeHttpClient : public auth::HttpClient
{
public:
    auth::HttpResult get(const std::string&) override
    {
        called = true;

        return {
            auth::HttpResult::Status::NetworkError,
            std::nullopt
        };
    }

    bool called = false;
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

    auth::JwksCache cache(std::chrono::seconds(60));
    cache.replace({key});

    FakeHttpClient httpClient;
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