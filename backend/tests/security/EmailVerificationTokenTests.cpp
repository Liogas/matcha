#include <gtest/gtest.h>

#include "security/EmailVerificationToken.hpp"

#include <set>
#include <string>

TEST(EmailVerificationTokenTest, GeneratesUniqueHexTokenAndHash)
{
	std::set<std::string> values;
	for (int i = 0; i < 100; ++i)
	{
		const auto token = generateEmailVerificationToken();
		ASSERT_EQ(token.value.size(), 64);
		ASSERT_EQ(token.hash.size(), 64);
		EXPECT_NE(token.value, token.hash);
		EXPECT_EQ(hashEmailVerificationToken(token.value), token.hash);
		EXPECT_TRUE(values.insert(token.value).second);
	}
}

TEST(EmailVerificationTokenTest, RejectsMalformedToken)
{
	EXPECT_THROW(hashEmailVerificationToken("not-a-token"), std::invalid_argument);
	EXPECT_THROW(
		hashEmailVerificationToken(std::string(64, 'A')),
		std::invalid_argument
	);
}
