#include <gtest/gtest.h>

#include <auth/jwks/JsonJwksParser.hpp>

TEST(JsonJwksParserTest, ParsesSingleRsaKey)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    ASSERT_EQ(
        result.status,
        auth::JwksParseResult::Status::Success
    );

    ASSERT_EQ(result.keys.size(), 1);

    EXPECT_EQ(result.keys[0].kid, "key-1");
    EXPECT_EQ(result.keys[0].kty, "RSA");
    EXPECT_EQ(result.keys[0].alg, "RS256");
    EXPECT_EQ(result.keys[0].n, "modulus-value");
    EXPECT_EQ(result.keys[0].e, "AQAB");
}

TEST(JsonJwksParserTest, RejectsJwksWithoutKeys)
{
    const std::string jwks = R"({
        "foo": "bar"
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsEmptyKeys)
{
    const std::string jwks = R"({
        "keys": []
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsNonRsaKey)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "EC",
                "alg": "ES256",
                "n": "modulus-value",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsKeyWithEmptyKid)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsKeyWithEmptyModulus)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsKeyWithEmptyExponent)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value",
                "e": ""
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsNonArrayKeys)
{
    const std::string jwks = R"({
        "keys": "not-an-array"
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsInvalidBase64UrlCharactersInModulus)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus+invalid",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsInvalidBase64UrlCharactersInExponent)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value",
                "e": "AQ+B"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsKeyWithEmptyAlgorithm)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "",
                "n": "modulus-value",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );

    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, ParsesMultipleRsaKeys)
{
    const std::string jwks = R"({
        "keys": [
            {
                "kid": "key-1",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value-1",
                "e": "AQAB"
            },
            {
                "kid": "key-2",
                "kty": "RSA",
                "alg": "RS256",
                "n": "modulus-value-2",
                "e": "AQAB"
            }
        ]
    })";

    auth::JsonJwksParser parser;

    const auto result = parser.parse(jwks);

    ASSERT_EQ(
        result.status,
        auth::JwksParseResult::Status::Success
    );

    ASSERT_EQ(result.keys.size(), 2);

    EXPECT_EQ(result.keys[0].kid, "key-1");
    EXPECT_EQ(result.keys[0].n, "modulus-value-1");

    EXPECT_EQ(result.keys[1].kid, "key-2");
    EXPECT_EQ(result.keys[1].n, "modulus-value-2");
}