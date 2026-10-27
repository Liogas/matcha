#include <core/Result.hpp>
#include <gtest/gtest.h>

namespace
{
	core::Result<int>	parsePositive(int value)
	{
		if (value <= 0)
			return core::fail(core::ErrorCode::Invalid, "must be positive");
		return value;
	}
}

TEST(Result, HoldsValueOnSuccess)
{
	const auto result = parsePositive(42);
	ASSERT_TRUE(result);
	EXPECT_EQ(*result, 42);
}

TEST(Result, HoldsErrorOnFailure)
{
	const auto result = parsePositive(-1);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(result.error().message, "must be positive");
}

TEST(Result, VoidResultHasNoValue)
{
	const core::Result<void> ok{};
	const core::Result<void> ko = core::fail(core::ErrorCode::Internal, "boom");
	EXPECT_TRUE(ok);
	EXPECT_FALSE(ko);
}

TEST(Error, CodesHaveStableNames)
{
	EXPECT_EQ(core::toString(core::ErrorCode::NotFound), "not_found");
	EXPECT_EQ(core::toString(core::ErrorCode::Unavailable), "unavailable");
}