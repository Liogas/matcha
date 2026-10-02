#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <sstream>

#include <auth/jwks/JwksProvider.hpp>
#include <auth/token/TokenValidator.hpp>

class FakeJwksProvider : public auth::JwksProvider
{
	public:
		explicit FakeJwksProvider(auth::JwksResult result):
			_result(std::move(result))
		{}

		auth::JwksResult getKey(const std::string &kid) override
		{
			requestedKid = kid;
			++callCount;
			return this->_result;
		}
		std::string requestedKid;
		int			callCount = 0;
	private:
		auth::JwksResult _result;
};

TEST(TokenValidatorTest, CanReadKidFromToken)
{
    const std::string token =
        jwt::create()
            .set_header_claim(
				"kid",
				jwt::claim(std::string("key-123"))
			)
            .set_payload_claim(
				"sub",
				jwt::claim(std::string("user-42"))
			)
            .sign(
                jwt::algorithm::none{}
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
        "modulus",
        "AQAB"
    };

    auth::JwksResult jwksResult{
        auth::JwksResult::Status::Success,
        jwk
    };

    FakeJwksProvider jwksProvider(jwksResult);

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        jwksProvider
    );

    const std::string token =
        jwt::create()
            .set_header_claim(
                "kid",
                jwt::claim(std::string("key-123"))
            )
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(
                jwt::algorithm::none{}
            );

    validator.validate(token);

    EXPECT_EQ(
        jwksProvider.requestedKid,
        "key-123"
    );
}

TEST(TokenValidatorTest, RejectsTokenWithoutKid)
{
    const auth::Jwk jwk{
        "key-123",
        "RSA",
        "RS256",
        "modulus",
        "AQAB"
    };

    const auth::JwksResult jwksResult{
        auth::JwksResult::Status::Success,
        jwk
    };

    FakeJwksProvider jwksProvider(jwksResult);

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        jwksProvider
    );

    const std::string token =
        jwt::create()
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(
                jwt::algorithm::none{}
            );

    const auto result = validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(
        jwksProvider.callCount,
        0
    );
}

TEST(TokenValidatorTest, ReturnsVerificationUnavailableWhenJwksIsUnavailable)
{
    const auth::JwksResult jwksResult{
        auth::JwksResult::Status::Unavailable,
        std::nullopt
    };

    FakeJwksProvider jwksProvider(jwksResult);

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        jwksProvider
    );

    const std::string token =
        jwt::create()
            .set_header_claim(
                "kid",
                jwt::claim(std::string("key-123"))
            )
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(
                jwt::algorithm::none{}
            );

    const auto result = validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::VerificationUnavailable
    );

    EXPECT_EQ(
        jwksProvider.callCount,
        1
    );
}

TEST(TokenValidatorTest, ReturnsInvalidTokenWhenJwkIsNotFound)
{
    const auth::JwksResult jwksResult{
        auth::JwksResult::Status::KeyNotFound,
        std::nullopt
    };

    FakeJwksProvider jwksProvider(jwksResult);

    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        jwksProvider
    );

    const std::string token =
        jwt::create()
            .set_header_claim(
                "kid",
                jwt::claim(std::string("unknown-key"))
            )
            .set_payload_claim(
                "sub",
                jwt::claim(std::string("user-42"))
            )
            .sign(jwt::algorithm::none{});

    const auto result = validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_EQ(jwksProvider.callCount, 1);
}

TEST(TokenValidatorTest, CreatesPublicKeyFromRsaComponents)
{
    const std::string modulus = 
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";
    const std::string exponent = "AQAB";

    const auto publicKey =
        jwt::helper::create_public_key_from_rsa_components(
            modulus,
            exponent
        );

    EXPECT_FALSE(publicKey.empty());
}