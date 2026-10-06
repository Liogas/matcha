#include <gtest/gtest.h>

#include <auth/auth/Authenticator.hpp>
#include <auth/token/TokenValidatorInterface.hpp>

#include <string>
#include <utility>

namespace
{
    class FakeTokenValidator :
        public auth::TokenValidatorInterface
    {
    public:
        explicit FakeTokenValidator(
            auth::TokenValidationResult result
        ):
            _result(std::move(result))
        {}

        auth::TokenValidationResult validate(
            const std::string &token
        ) override
        {
            lastToken = token;
            return _result;
        }

        std::string lastToken;

    private:
        auth::TokenValidationResult _result;
    };
}

TEST(AuthAuthenticationIntegrationTest, AuthenticatesValidToken)
{
    auth::AuthenticatedIdentity identity{
        "user-42",
        "https://issuer.example.com",
        {"matcha-api"}
    };

    const auth::TokenValidationResult expected{
        auth::TokenValidationResult::Status::Valid,
        identity
    };

    FakeTokenValidator validator(expected);

    auth::Authenticator authenticator(validator);

    const auto result =
        authenticator.authenticate("my-token");

    ASSERT_EQ(
        result.status,
        auth::TokenValidationResult::Status::Valid
    );

    ASSERT_TRUE(result.claims.has_value());

    EXPECT_EQ(
        result.claims->subject,
        "user-42"
    );

    EXPECT_EQ(
        result.claims->issuer,
        "https://issuer.example.com"
    );

    ASSERT_EQ(
        result.claims->audience.size(),
        1
    );

    EXPECT_EQ(
        result.claims->audience[0],
        "matcha-api"
    );

    EXPECT_EQ(
        validator.lastToken,
        "my-token"
    );
}