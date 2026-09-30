#include <gtest/gtest.h>

#include <auth/http/UrlParser.hpp>

TEST(UrlParserTests, ParsesHttpsUrl)
{
    const auto result =
        auth::UrlParser::parse(
            "https://example.com/keys"
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, "");
    EXPECT_EQ(result->path, "/keys");
}

TEST(UrlParserTests, ParsesHttpUrl)
{
    const auto result =
        auth::UrlParser::parse(
            "http://example.com/keys"
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->scheme, "http");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, "");
    EXPECT_EQ(result->path, "/keys");
}

TEST(UrlParserTests, ParsesUrlWithPort)
{
    const auto result =
        auth::UrlParser::parse(
            "https://example.com:8443/keys"
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, "8443");
    EXPECT_EQ(result->path, "/keys");
}

TEST(UrlParserTests, UsesRootPathWhenPathIsMissing)
{
    const auto result =
        auth::UrlParser::parse(
            "https://example.com"
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->scheme, "https");
    EXPECT_EQ(result->host, "example.com");
    EXPECT_EQ(result->port, "");
    EXPECT_EQ(result->path, "/");
}

TEST(UrlParserTests, RejectsUnsupportedScheme)
{
    const auto result =
        auth::UrlParser::parse(
            "ftp://example.com/keys"
        );

    EXPECT_FALSE(result.has_value());
}

TEST(UrlParserTests, RejectsUrlWithoutScheme)
{
    const auto result =
        auth::UrlParser::parse(
            "example.com/keys"
        );

    EXPECT_FALSE(result.has_value());
}
