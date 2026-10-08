#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <string>

#include <auth/token/TokenGenerator.hpp>

namespace
{
	class FakeTokenSigner : public auth::TokenSigner
	{
		public:
			std::string sign(
				const std::string &data
			) override
			{
				signedData = data;
				return "fake-signature";
			}
			std::string	keyId() const
			{
				return "key-123";
			}
			std::string signedData;
	};

	class FakeWallClock : public auth::IWallClock
	{
		public:
			explicit FakeWallClock(
				std::chrono::system_clock::time_point now
			):
				_now(now)
			{
			}

			std::chrono::system_clock::time_point now() const override
			{
				return _now;
			}

		private:
			std::chrono::system_clock::time_point _now;
	};

	nlohmann::json decodeJwtPart(const std::string &encoded)
	{
		return nlohmann::json::parse(
			jwt::base::decode<jwt::alphabet::base64url>(encoded)
		);
	}

	std::pair<std::string, std::string>
		splitSignedData(const std::string &signedData)
	{
		const auto separator = signedData.find('.');

		if (separator == std::string::npos)
			return {"", ""};

		return {
			signedData.substr(0, separator),
			signedData.substr(separator + 1)
		};
	}
}

TEST(TokenGeneratorTest, GeneratesTokenWithExpectedClaims)
{
    const auth::TokenGeneratorConfig config{
        "https://auth.cpp.local",
        "my-api",
        std::chrono::seconds{900}
    };

    const auto now =
        std::chrono::system_clock::time_point{
            std::chrono::seconds{1000}
        };

    FakeWallClock clock(now);
    FakeTokenSigner signer;

    auth::TokenGenerator generator(
        config,
        signer,
        clock
    );

    const auth::AuthenticatedIdentity identity{
        "user-42",
        "",
        {}
    };

    const auto token = generator.generate(identity);

    EXPECT_FALSE(token.empty());

    const auto [encodedHeader, encodedPayload] =
        splitSignedData(signer.signedData);

    ASSERT_FALSE(encodedHeader.empty());
    ASSERT_FALSE(encodedPayload.empty());

    const auto header = decodeJwtPart(encodedHeader);
    const auto payload = decodeJwtPart(encodedPayload);

    EXPECT_EQ(header["alg"], "RS256");
    EXPECT_EQ(header["typ"], "JWT");
	EXPECT_EQ(header["kid"], "key-123");

    EXPECT_EQ(payload["iss"], "https://auth.cpp.local");
    EXPECT_EQ(payload["sub"], "user-42");
    EXPECT_EQ(payload["aud"], "my-api");
    EXPECT_EQ(payload["iat"], 1000);
    EXPECT_EQ(payload["exp"], 1900);

    EXPECT_EQ(token, signer.signedData + ".fake-signature");
}

