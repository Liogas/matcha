#include <gtest/gtest.h>

#include <auth/token/TokenValidatorInterface.hpp>

#include <utility>

namespace
{
	class FakeTokenValidator :
		public auth::TokenValidatorInterface
	{
		public:
			explicit FakeTokenValidator(
				auth::TokenValidationResult	result
			):
				_result(std::move(result))
			{}

			auth::TokenValidationResult	validate(
				const std::string &token
			) override
			{
				lastToken = token;
				return this->_result;
			}
			std::string lastToken;
		private:
			auth::TokenValidationResult	_result;
	};
}

TEST(TokenValidatorInterfaceTest, ValidatesToken)
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

    const auto result =
        validator.validate("my-token");

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
        validator.lastToken,
        "my-token"
    );
}