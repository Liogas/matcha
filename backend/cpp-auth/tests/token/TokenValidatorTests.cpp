#include <auth/token/TokenValidator.hpp>
#include <auth/jwks/JwksProvider.hpp>

#include <gtest/gtest.h>

#include <jwt-cpp/jwt.h>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace
{
    std::string createTestToken(
        const std::string& issuer,
        const std::optional<std::string>& kid = std::nullopt
    )
    {
        std::ifstream keyFile(
            std::string(CPP_AUTH_SOURCE_DIR)
            + "/tests/certs/test-key.pem"
        );

        EXPECT_TRUE(keyFile.is_open());

        std::stringstream buffer;
        buffer << keyFile.rdbuf();

        const std::string privateKey = buffer.str();

        const std::string modulus =
            "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

        const std::string exponent = "AQAB";

        const auto publicKey =
            jwt::helper::create_public_key_from_rsa_components(
                modulus,
                exponent
            );

        const auto signer =
            jwt::algorithm::rs256(
                publicKey,
                privateKey
            );

        auto builder =
            jwt::create()
                .set_payload_claim(
                    "iss",
                    jwt::claim(issuer)
                )
                .set_payload_claim(
                    "sub",
                    jwt::claim(std::string("user-42"))
                );

        if (kid.has_value())
        {
            builder.set_header_claim(
                "kid",
                jwt::claim(kid.value())
            );
        }

        return builder.sign(signer);
    }

    std::string createTestTokenWithoutIssuer(
        const std::string& kid
    )
    {
        std::ifstream keyFile(
            std::string(CPP_AUTH_SOURCE_DIR)
            + "/tests/certs/test-key.pem"
        );

        EXPECT_TRUE(keyFile.is_open());

        std::stringstream buffer;
        buffer << keyFile.rdbuf();

        const std::string privateKey = buffer.str();

        const std::string modulus =
            "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

        const std::string exponent = "AQAB";

        const auto publicKey =
            jwt::helper::create_public_key_from_rsa_components(
                modulus,
                exponent
            );

        const auto signer =
            jwt::algorithm::rs256(
                publicKey,
                privateKey
            );

        return jwt::create()
            .set_header_claim(
                "kid",
                jwt::claim(kid)
            )
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(signer);
    }
}

class FakeJwksProvider : public auth::JwksProvider
{
public:
    explicit FakeJwksProvider(auth::JwksResult result):
        _result(std::move(result))
    {}

    auth::JwksResult getKey(const std::string& kid) override
    {
        requestedKid = kid;
        ++callCount;

        return this->_result;
    }

    std::string requestedKid;
    int callCount = 0;

private:
    auth::JwksResult _result;
};

TEST(TokenValidatorTest, CanReadKidFromToken)
{
    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const auto decoded = jwt::decode(token);

    EXPECT_EQ(
        decoded.get_header_claim("kid").as_string(),
        "key-123"
    );
}

TEST(TokenValidatorTest, RequestsJwkUsingTokenKid)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    validator.validate(token);

    EXPECT_EQ(
        provider.requestedKid,
        "key-123"
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(TokenValidatorTest, RejectsTokenWithoutKid)
{
    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::KeyNotFound,
            std::nullopt
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        0
    );
}

TEST(
    TokenValidatorTest,
    ReturnsVerificationUnavailableWhenJwksIsUnavailable
)
{
    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Unavailable,
            std::nullopt
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::VerificationUnavailable
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(
    TokenValidatorTest,
    ReturnsInvalidTokenWhenJwkIsNotFound
)
{
    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::KeyNotFound,
            std::nullopt
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(TokenValidatorTest, CreatesPublicKeyFromRsaComponents)
{
    const std::string modulus =
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

    const std::string exponent = "AQAB";

    EXPECT_NO_THROW(
        jwt::helper::create_public_key_from_rsa_components(
            modulus,
            exponent
        )
    );
}

TEST(TokenValidatorTest, VerifiesRsaSignaturesUsingJwkPublicKey)
{
    std::ifstream keyFile(
        std::string(CPP_AUTH_SOURCE_DIR)
        + "/tests/certs/test-key.pem"
    );

    ASSERT_TRUE(keyFile.is_open());

    std::stringstream buffer;
    buffer << keyFile.rdbuf();

    const std::string privateKey = buffer.str();

    const std::string modulus =
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

    const std::string exponent = "AQAB";

    const auto publicKey =
        jwt::helper::create_public_key_from_rsa_components(
            modulus,
            exponent
        );

    const auto signer =
        jwt::algorithm::rs256(
            publicKey,
            privateKey
        );

    const auto token =
        jwt::create()
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(signer);

    const auto decoded =
        jwt::decode(token);

    EXPECT_NO_THROW(
        jwt::verify()
            .allow_algorithm(
                jwt::algorithm::rs256(publicKey)
            )
            .verify(decoded)
    );
}

TEST(TokenValidatorTest, AcceptsValidRsaToken)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(TokenValidatorTest, RejectsInvalidRsaSignature)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const std::size_t signatureStart =
        token.find_last_of('.');

    ASSERT_NE(
        signatureStart,
        std::string::npos
    );

    const std::size_t characterToModify =
        signatureStart + 1;

    ASSERT_LT(
        characterToModify,
        token.size()
    );

    token[characterToModify] =
        token[characterToModify] == 'a'
            ? 'b'
            : 'a';

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(
    TokenValidatorTest,
    RejectsTokenWhenConfiguredAlgorithmDoesNotMatch
)
{
    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::KeyNotFound,
            std::nullopt
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        jwt::create()
            .set_algorithm("HS256")
            .set_header_claim(
                "kid",
                jwt::claim(std::string("key-123"))
            )
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(
                jwt::algorithm::hs256{
                    "test-secret"
                }
            );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        0
    );
}

TEST(TokenValidatorTest, AcceptsTokenWithExpectedIssuer)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://issuer.example.com",
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    EXPECT_EQ(
        provider.callCount,
        1
    );
}

TEST(TokenValidatorTest, RejectsTokenWithUnexpectedIssuer)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestToken(
            "https://evil-issuer.example.com",
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        0
    );
}

TEST(TokenValidatorTest, RejectsTokenWithoutIssuer)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
        "AQAB"
    };

    FakeJwksProvider provider{
        {
            auth::JwksResult::Status::Success,
            jwk
        }
    };

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider
    );

    const std::string token =
        createTestTokenWithoutIssuer(
            "key-123"
        );

    const auto result =
        validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        provider.callCount,
        0
    );
}