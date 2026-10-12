
#include <gtest/gtest.h>

#include <auth/jwks/JsonJwksParser.hpp>

#include <string>

namespace
{
    // 342 caractères Base64URL, soit 256 octets décodés.
    // Suffisant pour tester la règle de taille du parseur,
    // mais pas une véritable clé RSA utilisable en cryptographie.
    std::string validRsaModulus()
    {
        return std::string(342, 'w');
    }

    std::string makeRsaKey(
        const std::string &kid,
        const std::string &modulus,
        const std::string &exponent = "AQAB",
        const std::string &algorithm = "RS256",
        const std::string &keyType = "RSA"
    )
    {
        return R"({"kid":")" + kid
            + R"(","kty":")" + keyType
            + R"(","alg":")" + algorithm
            + R"(","n":")" + modulus
            + R"(","e":")" + exponent + R"("})";
    }

    std::string makeJwks(const std::string &keys)
    {
        return R"({"keys":[)" + keys + R"(]})";
    }
}

TEST(JsonJwksParserTest, ParsesSingleRsaKey)
{
    const std::string modulus = validRsaModulus();
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", modulus)
    );

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
    EXPECT_EQ(result.keys[0].n, modulus);
    EXPECT_EQ(result.keys[0].e, "AQAB");
}

TEST(JsonJwksParserTest, RejectsJwksWithoutKeys)
{
    const std::string jwks = R"({"foo":"bar"})";

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
    const std::string jwks = R"({"keys":[]})";

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
    const std::string jwks = makeJwks(
        makeRsaKey(
            "key-1",
            validRsaModulus(),
            "AQAB",
            "ES256",
            "EC"
        )
    );

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
    const std::string jwks = makeJwks(
        makeRsaKey("", validRsaModulus())
    );

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
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", "")
    );

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
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", validRsaModulus(), "")
    );

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
    const std::string jwks = R"({"keys":"not-an-array"})";

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
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", "modulus+invalid")
    );

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
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", validRsaModulus(), "AQ+B")
    );

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
    const std::string jwks = makeJwks(
        makeRsaKey(
            "key-1",
            validRsaModulus(),
            "AQAB",
            ""
        )
    );

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
    const std::string modulus1 = validRsaModulus();
    const std::string modulus2 = validRsaModulus();

    const std::string jwks = makeJwks(
        makeRsaKey("key-1", modulus1)
        + ","
        + makeRsaKey("key-2", modulus2)
    );

    auth::JsonJwksParser parser;
    const auto result = parser.parse(jwks);

    ASSERT_EQ(
        result.status,
        auth::JwksParseResult::Status::Success
    );
    ASSERT_EQ(result.keys.size(), 2);

    EXPECT_EQ(result.keys[0].kid, "key-1");
    EXPECT_EQ(result.keys[0].n, modulus1);

    EXPECT_EQ(result.keys[1].kid, "key-2");
    EXPECT_EQ(result.keys[1].n, modulus2);
}

TEST(JsonJwksParserTest, RejectsNonObjectKey)
{
    const std::string jwks = R"({"keys":["not-a-key"]})";

    auth::JsonJwksParser parser;
    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );
    EXPECT_TRUE(result.keys.empty());
}

TEST(JsonJwksParserTest, RejectsRsaModulusTooShort)
{
    const std::string jwks = makeJwks(
        makeRsaKey("key-1", "YWJj")
    );

    auth::JsonJwksParser parser;
    const auto result = parser.parse(jwks);

    EXPECT_EQ(
        result.status,
        auth::JwksParseResult::Status::InvalidJwks
    );
    EXPECT_TRUE(result.keys.empty());
}
