#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

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