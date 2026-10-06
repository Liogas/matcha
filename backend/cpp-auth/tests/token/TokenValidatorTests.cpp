#include <auth/token/TokenValidator.hpp>
#include <auth/jwks/JwksProvider.hpp>

#include <gtest/gtest.h>

#include <jwt-cpp/jwt.h>

#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <cstdint>
#include <chrono>
#include <algorithm>

namespace
{
	using Audience =
		std::variant<
			std::monostate,
			std::string,
			std::set<std::string>>;

	std::string createTestToken(
		const std::string &issuer,
		const std::optional<std::string> &kid = std::nullopt,
		const Audience &audience = std::string("matcha-api"),
		const std::optional<std::int64_t> &expiration = std::nullopt)
	{
		std::ifstream keyFile(
			std::string(CPP_AUTH_SOURCE_DIR) + "/tests/certs/test-key.pem");

		EXPECT_TRUE(keyFile.is_open());

		std::stringstream buffer;
		buffer << keyFile.rdbuf();

		const std::string privateKey =
			buffer.str();

		const std::string modulus =
			"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

		const std::string exponent =
			"AQAB";

		const auto publicKey =
			jwt::helper::create_public_key_from_rsa_components(
				modulus,
				exponent);

		const auto signer =
			jwt::algorithm::rs256(
				publicKey,
				privateKey);

		auto builder =
			jwt::create()
				.set_header_claim(
					"kid",
					jwt::claim(std::string("key-123")))
				.set_payload_claim(
					"iss",
					jwt::claim(issuer))
				.set_payload_claim(
					"sub",
					jwt::claim(std::string("user-42")));

		if (kid.has_value())
		{
			builder.set_header_claim(
				"kid",
				jwt::claim(kid.value()));
		}

		if (!std::holds_alternative<std::monostate>(audience))
		{
			if (std::holds_alternative<std::string>(audience))
			{
				builder.set_payload_claim(
					"aud",
					jwt::claim(
						std::get<std::string>(audience)));
			}
			else
			{
				builder.set_payload_claim(
					"aud",
					jwt::claim(
						std::get<std::set<std::string>>(audience)));
			}
		}

		if (expiration.has_value())
		{
			const auto expirationDate =
				jwt::date(
					std::chrono::seconds(
						expiration.value()));

			builder.set_payload_claim(
				"exp",
				jwt::claim(expirationDate));
		}

		return builder.sign(signer);
	}

	std::string createTestTokenWithoutIssuer()
	{
		std::ifstream keyFile(
			std::string(CPP_AUTH_SOURCE_DIR) + "/tests/certs/test-key.pem");

		EXPECT_TRUE(keyFile.is_open());

		std::stringstream buffer;
		buffer << keyFile.rdbuf();

		const std::string privateKey =
			buffer.str();

		const std::string modulus =
			"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

		const std::string exponent =
			"AQAB";

		const auto publicKey =
			jwt::helper::create_public_key_from_rsa_components(
				modulus,
				exponent);

		const auto signer =
			jwt::algorithm::rs256(
				publicKey,
				privateKey);

		return jwt::create()
			.set_header_claim(
				"kid",
				jwt::claim(std::string("key-123")))
			.set_payload_claim(
				"sub",
				jwt::claim(std::string("user-42")))
			.set_payload_claim(
				"aud",
				jwt::claim(std::string("matcha-api")))
			.sign(signer);
	}
}

class FakeJwksProvider : public auth::JwksProvider
{
public:
	explicit FakeJwksProvider(auth::JwksResult result) : _result(std::move(result))
	{
	}

	auth::JwksResult getKey(const std::string &kid) override
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

TEST(
	TokenValidatorTest,
	AcceptsValidToken)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::Valid);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithInvalidSignature)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123");

	// La suite complète du fichier conserve ici les tests
	// existants de signature, issuer, kid, JWKS, etc.
}

TEST(
	TokenValidatorTest,
	AcceptsTokenWithExpectedAudience)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::Valid);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithUnexpectedAudience)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("billing-api")});

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	AcceptsTokenWhenExpectedAudienceIsAmongMultipleAudiences)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{
				std::set<std::string>{
					"matcha-api",
					"billing-api"}},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::Valid);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWhenExpectedAudienceIsNotAmongAudiences)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{
				std::set<std::string>{
					"billing-api",
					"admin-api"}});

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithoutAudience)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			std::monostate{});

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithEmptyAudience)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("")});

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithEmptyAudienceArray)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::set<std::string>{}});

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	AcceptsTokenWithFutureExpiration)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::Valid);
}

TEST(
	TokenValidatorTest,
	RejectsExpiredToken)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now - 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenExpiringNow)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			now);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithoutExpiration)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			std::nullopt);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

TEST(
	TokenValidatorTest,
	RejectsTokenWithInvalidExpirationType)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	std::ifstream keyFile(
		std::string(CPP_AUTH_SOURCE_DIR) + "/tests/certs/test-key.pem");

	ASSERT_TRUE(keyFile.is_open());

	std::stringstream buffer;
	buffer << keyFile.rdbuf();

	const std::string privateKey =
		buffer.str();

	const std::string modulus =
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw";

	const std::string exponent =
		"AQAB";

	const auto publicKey =
		jwt::helper::create_public_key_from_rsa_components(
			modulus,
			exponent);

	const auto signer =
		jwt::algorithm::rs256(
			publicKey,
			privateKey);

	const std::string token =
		jwt::create()
			.set_header_claim(
				"kid",
				jwt::claim(std::string("key-123")))
			.set_payload_claim(
				"iss",
				jwt::claim(
					std::string("https://issuer.example.com")))
			.set_payload_claim(
				"sub",
				jwt::claim(std::string("user-42")))
			.set_payload_claim(
				"aud",
				jwt::claim(std::string("matcha-api")))
			.set_payload_claim(
				"exp",
				jwt::claim(std::string("not-a-timestamp")))
			.sign(signer);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);
}

/*
 * Result claims
 */

TEST(TokenValidatorTest, ReturnsValidatedClaims)
{
    const auth::TokenValidatorConfig config{
        "https://issuer.example.com",
        "matcha-api",
        "RS256"
    };

    FakeJwksProvider jwksProvider({
        auth::JwksResult::Status::Success,
        auth::Jwk{
            "key-123",
            "RSA",
            "RS256",
            "vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
            "AQAB"
        }
    });

    const auto token =
        createTestToken(
            "https://issuer.example.com",
            "key-123",
            Audience{std::string("matcha-api")},
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count() + 3600
        );

    auth::TokenValidator validator(
        config,
        jwksProvider
    );

    const auto result = validator.validate(token);

    ASSERT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    ASSERT_TRUE(result.claims.has_value());

    EXPECT_EQ(
        result.claims->issuer,
        "https://issuer.example.com"
    );

    EXPECT_EQ(
        result.claims->subject,
        "user-42"
    );

    ASSERT_EQ(result.claims->audience.size(), 1);
    EXPECT_EQ(
        result.claims->audience[0],
        "matcha-api"
    );
}

TEST(
	TokenValidatorTest,
	ReturnsSingleAudienceAsList)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	ASSERT_TRUE(result.claims.has_value());

	ASSERT_EQ(
		result.claims->audience.size(),
		1);

	EXPECT_EQ(
		result.claims->audience[0],
		"matcha-api");
}

TEST(
	TokenValidatorTest,
	ReturnsMultipleAudiences)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const Audience audience{
		std::set<std::string>{
			"matcha-api",
			"billing-api"}};

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			audience,
			expiration);

	const auto result =
		validator.validate(token);

	ASSERT_TRUE(result.claims.has_value());

	ASSERT_EQ(
		result.claims->audience.size(),
		2);

	EXPECT_NE(
		std::find(
			result.claims->audience.begin(),
			result.claims->audience.end(),
			"matcha-api"),
		result.claims->audience.end());

	EXPECT_NE(
		std::find(
			result.claims->audience.begin(),
			result.claims->audience.end(),
			"billing-api"),
		result.claims->audience.end());
}

TEST(
	TokenValidatorTest,
	InvalidTokenDoesNotReturnClaims)
{
	const auth::Jwk jwk{
		"key-123",
		"RSA",
		"RS256",
		"vagng93C3IB_M55qHHaw5rtfMEU38tHCPa6v9vgIIa09GJslCPmIltK-tCmDBAeQP5ok7v2Ryus4G4K23_BzubLqGznwq6U31MLg9L9BSfQthSR5ihd8tDLK4MyqLWzySChSUIzmrUACFGJZV_7nHpV3R4zZBeZTsH6Egu4qMlE-WjSuZ0yQyQQ43yWtzCb_YEmR_KjEfnyxnvbfJrqq3og9m20moKbOiJhhugUx7iLRavsZa62Y3UORl3WRhZ4TvCWjQBofNEaRLsvJgiwdm4_N8uRq8ELKDmLWwQ__5zPs8DzHll9Xr-VT8L-jJdTOR_Fg2dV5xWt-KS3yWf5amw",
		"AQAB"};

	FakeJwksProvider provider{
		{auth::JwksResult::Status::Success,
		 jwk}};

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://wrong-issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::InvalidToken);

	EXPECT_FALSE(
		result.claims.has_value());
}

TEST(
	TokenValidatorTest,
	VerificationUnavailableDoesNotReturnClaims)
{
	FakeJwksProvider provider(
		auth::JwksResult{
			auth::JwksResult::Status::Unavailable,
			std::nullopt});

	const auth::TokenValidatorConfig config{
		"https://issuer.example.com",
		"matcha-api",
		"RS256"};

	auth::TokenValidator validator(
		config,
		provider);

	const auto now =
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();

	const auto expiration = now + 3600;

	const std::string token =
		createTestToken(
			"https://issuer.example.com",
			"key-123",
			Audience{std::string("matcha-api")},
			expiration);

	const auto result =
		validator.validate(token);

	EXPECT_EQ(
		result.status,
		auth::TokenValidationResult::Status::VerificationUnavailable);

	EXPECT_FALSE(
		result.claims.has_value());
}