#include <gtest/gtest.h>
#include "FakeHttpClient.hpp"

TEST(HttpClientTest, ReturnsConfiguredResult)
{
    auth::HttpResponse response{
        200,
        R"({"hello":"world"})"
    };

    auth::HttpResult expected{
        auth::HttpResult::Status::Success,
        response
    };

    FakeHttpClient client(expected);

    auto result = client.get("https://example.com");

    ASSERT_EQ(result.status, auth::HttpResult::Status::Success);
    ASSERT_TRUE(result.response.has_value());

    EXPECT_EQ(result.response->statusCode, 200);
    EXPECT_EQ(result.response->body, R"({"hello":"world"})");
}

TEST(HttpClientTest, ReturnsNetworkError)
{
    auth::HttpResult expected{
        auth::HttpResult::Status::NetworkError,
        std::nullopt
    };

    FakeHttpClient client(expected);

    auto result = client.get("https://example.com");

    EXPECT_EQ(
        result.status,
        auth::HttpResult::Status::NetworkError
    );

    EXPECT_FALSE(result.response.has_value());
}