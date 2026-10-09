#include <auth/token/RsaTokenSignatureVerifier.hpp>
#include <auth/token/TokenValidator.hpp>
#include <auth/jwks/JwksProvider.hpp>

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <stdexcept>
#include <algorithm>

namespace
{
	using Audience = std::variant<
		std::monostate,
		std::string,
		std::set<std::string>
	>;

	constexpr char TEST_KEY_ID[] = "key-123";
	constexpr char TEST_ISSUER[] = "https://issuer.example.com";
	constexpr char TEST_AUDIENCE[] = "matcha-api";
	constexpr char TEST_SUBJECT[] = "user-42";

	constexpr char TEST_RSA_MODULUS[] =
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-"
		"tCmDBAeQP5ok7v2Ryus4G4K23_"
		"BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_"
		"7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_"
		"KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRL"
		"svJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-"
		"KS3yWf5amw";

	constexpr char TEST_RSA_EXPONENT[] = "AQAB";

	std::string loadPrivateKey()
	{
		std::ifstream keyFile(
			std::string(CPP_AUTH_SOURCE_DIR) + "/tests/certs/test-key.pem"
		);

		if (!keyFile.is_open())
		{
			throw std::runtime_error(
				"Unable to open the test RSA private key"
			);
		}

		std::stringstream buffer;
		buffer << keyFile.rdbuf();

		return buffer.str();
	}

	std::string createTestToken(
		const std::string& issuer,
		const std::optional<std::string>& kid = std::nullopt,
		const Audience& audience = std::string(TEST_AUDIENCE),
		const std::optional<std::int64_t>& expiration = std::nullopt
	)
	{
		const std::string privateKey = loadPrivateKey();

		const auto publicKey =
			jwt::helper::create_public_key_from_rsa_components(
				TEST_RSA_MODULUS,
				TEST_RSA_EXPONENT
			);

		const auto signer = jwt::algorithm::rs256(
			publicKey,
			privateKey
		);

		auto builder = jwt::create()
			.set_payload_claim(
				"iss",
				jwt::claim(issuer)
			)
			.set_payload_claim(
				"sub",
				jwt::claim(std::string(TEST_SUBJECT))
			);
		if (kid.has_value())
		{
			builder.set_header_claim(
				"kid",
				jwt::claim(kid.value())
			);
		}
		if (!std::holds_alternative<std::monostate>(audience))
		{
			if (std::holds_alternative<std::string>(audience))
			{
				builder.set_payload_claim(
					"aud",
					jwt::claim(std::get<std::string>(audience))
				);
			}
			else
			{
				builder.set_payload_claim(
					"aud",
					jwt::claim(std::get<std::set<std::string>>(audience))
				);
			}
		}
		if (expiration.has_value())
		{
			const auto expirationDate = jwt::date(
				std::chrono::seconds(expiration.value())
			);

			builder.set_payload_claim(
				"exp",
				jwt::claim(expirationDate)
			);
		}
		return builder.sign(signer);
	}

	class FakeJwksProvider : public auth::JwksProvider
	{
	public:
		explicit FakeJwksProvider(auth::JwksResult result)
			: _result(std::move(result))
		{
		}

		auth::JwksResult getKey(const std::string& kid) override
		{
			requestedKid = kid;
			++callCount;

			return _result;
		}

		std::string requestedKid;
		int callCount = 0;

	private:
		auth::JwksResult _result;
	};

	auth::TokenValidationResult validateTestToken(
		const std::string& token,
		auth::JwksResult jwksResult,
		auth::TokenValidatorConfig config = {
			TEST_ISSUER,
			TEST_AUDIENCE,
			"RS256"
		}
	)
	{
		FakeJwksProvider provider{std::move(jwksResult)};
		auth::RsaTokenSignatureVerifier signatureVerifier;

		auth::TokenValidator validator(
			config,
			provider,
			signatureVerifier
		);

		return validator.validate(token);
	}

	std::string loadOtherPrivateKey()
	{
		std::ifstream keyFile(
			std::string(CPP_AUTH_SOURCE_DIR)
				+ "/tests/certs/other-test-key.pem"
		);

		if (!keyFile.is_open())
		{
			throw std::runtime_error(
				"Unable to open the other test RSA private key"
			);
		}

		std::stringstream buffer;
		buffer << keyFile.rdbuf();

		return buffer.str();
	}

	std::string loadOtherPublicKey()
	{
		std::ifstream file(
			std::string(CPP_AUTH_SOURCE_DIR)
				+ "/tests/certs/other-test-public.pem"
		);

		if (!file)
			throw std::runtime_error("Failed to open other test public key");

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
}

TEST(TokenValidatorTest, AcceptsValidToken)
{
    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    FakeJwksProvider provider{
        {auth::JwksResult::Status::Success, jwk}
    };

    const auth::TokenValidatorConfig config{
        TEST_ISSUER,
        TEST_AUDIENCE,
        "RS256"
    };

    auth::RsaTokenSignatureVerifier signatureVerifier;

    auth::TokenValidator validator(
        config,
        provider,
        signatureVerifier
    );

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        now + 3600
    );

    const auto result = validator.validate(token);

    ASSERT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    ASSERT_TRUE(result.claims.has_value());

    EXPECT_EQ(result.claims->issuer, TEST_ISSUER);
    EXPECT_EQ(result.claims->subject, TEST_SUBJECT);

    ASSERT_EQ(result.claims->audience.size(), 1);
    EXPECT_EQ(result.claims->audience[0], TEST_AUDIENCE);

    EXPECT_EQ(provider.callCount, 1);
    EXPECT_EQ(provider.requestedKid, TEST_KEY_ID);
}

TEST(TokenValidatorTest, RejectsEmptyToken)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        "",
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsMalformedToken)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        "not.a.valid.jwt",
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsWrongIssuer)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        "https://wrong-issuer.example.com",
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)}
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsWrongAudience)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string("another-api")}
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, AcceptsAudienceArrayContainingExpectedAudience)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };
	const auto now = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::set<std::string>{
            TEST_AUDIENCE,
            "another-api"
        }},
		now + 3600
    );
    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    ASSERT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    ASSERT_TRUE(result.claims.has_value());
    EXPECT_EQ(result.claims->audience.size(), 2);
	EXPECT_NE(
		std::find(
			result.claims->audience.begin(),
			result.claims->audience.end(),
			TEST_AUDIENCE
		),
		result.claims->audience.end()
	);
	EXPECT_NE(
		std::find(
			result.claims->audience.begin(),
			result.claims->audience.end(),
			"another-api"
		),
		result.claims->audience.end()
	);
}

TEST(TokenValidatorTest, RejectsAudienceArrayWithoutExpectedAudience)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::set<std::string>{
            "another-api",
            "some-other-api"
        }}
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsExpiredToken)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        now - 3600
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsTokenWithoutExpiration)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)}
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsTokenWithoutKeyId)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        std::nullopt,
        Audience{std::string(TEST_AUDIENCE)}
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, RejectsWrongConfiguredAlgorithm)
{
    const auth::Jwk jwk{
        TEST_KEY_ID, "RSA", "RS256",
        TEST_RSA_MODULUS, TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)}
    );

    const auth::TokenValidatorConfig config{
        TEST_ISSUER,
        TEST_AUDIENCE,
        "HS256"
    };

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk},
        config
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
}

TEST(TokenValidatorTest, ReturnsVerificationUnavailableWhenJwksIsUnavailable)
{
    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + 3600
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Unavailable, std::nullopt}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::VerificationUnavailable
    );

    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWhenJwksKeyIsNotFound)
{
    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + 3600
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::KeyNotFound, std::nullopt}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWhenJwksSuccessHasNoKey)
{
    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + 3600
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, std::nullopt}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWhenJwksKeyAlgorithmDoesNotMatch)
{
    const auth::Jwk mismatchedJwk{
        TEST_KEY_ID,
        "RSA",
        "RS384",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + 3600
    );

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, mismatchedJwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWithModifiedSignature)
{
    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    std::string token = createTestToken(
        TEST_ISSUER,
        TEST_KEY_ID,
        Audience{std::string(TEST_AUDIENCE)},
        now + 3600
    );

    const std::size_t signatureStart = token.rfind('.');

    ASSERT_NE(signatureStart, std::string::npos);
    ASSERT_LT(signatureStart + 1, token.size());

    const std::size_t characterToModify = signatureStart + 1;

    token[characterToModify] =
        token[characterToModify] == 'A' ? 'B' : 'A';

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );

    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RequestsJwkUsingTokenKeyId)
{
    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const std::string requestedKid = "unknown-key-456";

    const std::string token = createTestToken(
        TEST_ISSUER,
        requestedKid,
        Audience{std::string(TEST_AUDIENCE)},
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() + 3600
    );

    FakeJwksProvider provider{
        {auth::JwksResult::Status::KeyNotFound, std::nullopt}
    };

    auth::RsaTokenSignatureVerifier signatureVerifier;

    const auth::TokenValidatorConfig config{
        TEST_ISSUER,
        TEST_AUDIENCE,
        "RS256"
    };

    auth::TokenValidator validator(
        config,
        provider,
        signatureVerifier
    );

    const auto result = validator.validate(token);

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
    EXPECT_EQ(provider.callCount, 1);
    EXPECT_EQ(provider.requestedKid, requestedKid);
}

TEST(TokenValidatorTest, RejectsTokenWithoutIssuer)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string privateKey = loadPrivateKey();

    const auto publicKey =
        jwt::helper::create_public_key_from_rsa_components(
            TEST_RSA_MODULUS,
            TEST_RSA_EXPONENT
        );

    const auto signer = jwt::algorithm::rs256(publicKey, privateKey);

    const std::string token = jwt::create()
        .set_header_claim("kid", jwt::claim(std::string(TEST_KEY_ID)))
        .set_payload_claim("sub", jwt::claim(std::string(TEST_SUBJECT)))
        .set_payload_claim(
            "aud",
            jwt::claim(std::string(TEST_AUDIENCE))
        )
        .set_payload_claim(
            "exp",
            jwt::claim(jwt::date(std::chrono::seconds(now + 3600)))
        )
        .sign(signer);

    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWithoutAudience)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string privateKey = loadPrivateKey();

    const auto publicKey =
        jwt::helper::create_public_key_from_rsa_components(
            TEST_RSA_MODULUS,
            TEST_RSA_EXPONENT
        );

    const auto signer = jwt::algorithm::rs256(publicKey, privateKey);

    const std::string token = jwt::create()
        .set_header_claim("kid", jwt::claim(std::string(TEST_KEY_ID)))
        .set_payload_claim("iss", jwt::claim(std::string(TEST_ISSUER)))
        .set_payload_claim("sub", jwt::claim(std::string(TEST_SUBJECT)))
        .set_payload_claim(
            "exp",
            jwt::claim(jwt::date(std::chrono::seconds(now + 3600)))
        )
        .sign(signer);

    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenWithoutSubject)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string privateKey = loadPrivateKey();

    const auto publicKey =
        jwt::helper::create_public_key_from_rsa_components(
            TEST_RSA_MODULUS,
            TEST_RSA_EXPONENT
        );

    const auto signer = jwt::algorithm::rs256(publicKey, privateKey);

    const std::string token = jwt::create()
        .set_header_claim("kid", jwt::claim(std::string(TEST_KEY_ID)))
        .set_payload_claim("iss", jwt::claim(std::string(TEST_ISSUER)))
        .set_payload_claim(
            "aud",
            jwt::claim(std::string(TEST_AUDIENCE))
        )
        .set_payload_claim(
            "exp",
            jwt::claim(jwt::date(std::chrono::seconds(now + 3600)))
        )
        .sign(signer);

    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
    EXPECT_FALSE(result.claims.has_value());
}

TEST(TokenValidatorTest, RejectsTokenSignedWithDifferentPrivateKey)
{
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    const std::string otherPrivateKey = loadOtherPrivateKey();
	const std::string otherPublicKey = loadOtherPublicKey();

    const auto otherSigner = jwt::algorithm::rs256(
        otherPublicKey,
        otherPrivateKey
    );

    const std::string token = jwt::create()
        .set_header_claim("kid", jwt::claim(std::string(TEST_KEY_ID)))
        .set_payload_claim("iss", jwt::claim(std::string(TEST_ISSUER)))
        .set_payload_claim("sub", jwt::claim(std::string(TEST_SUBJECT)))
        .set_payload_claim(
            "aud",
            jwt::claim(std::string(TEST_AUDIENCE))
        )
        .set_payload_claim(
            "exp",
            jwt::claim(jwt::date(std::chrono::seconds(now + 3600)))
        )
        .sign(otherSigner);

    const auth::Jwk jwk{
        TEST_KEY_ID,
        "RSA",
        "RS256",
        TEST_RSA_MODULUS,
        TEST_RSA_EXPONENT
    };

    const auto result = validateTestToken(
        token,
        {auth::JwksResult::Status::Success, jwk}
    );

    EXPECT_EQ(
        result.status,
        auth::TokenValidationResult::Status::InvalidToken
    );
    EXPECT_FALSE(result.claims.has_value());
}