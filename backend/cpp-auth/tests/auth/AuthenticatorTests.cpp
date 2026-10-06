#include <auth/auth/Authenticator.hpp>
#include <gtest/gtest.h>

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

TEST(AuthenticatorTest, DelegatesTokenValidation)
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

    EXPECT_EQ(
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

    EXPECT_EQ(
        validator.lastToken,
        "my-token"
    );
}
