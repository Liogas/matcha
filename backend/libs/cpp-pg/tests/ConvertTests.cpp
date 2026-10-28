#include <pg/Convert.hpp>
#include <gtest/gtest.h>

using namespace std::chrono;

TEST(Convert, ScalarsToText)
{
	EXPECT_EQ(pg::toText(true), "true");
	EXPECT_EQ(pg::toText(42), "42");
	EXPECT_EQ(pg::toText(std::int64_t{-9000000000}), "-9000000000");
	EXPECT_EQ(pg::toText(2.5), "2.5");
}

TEST(Convert, StringLiteralStaysAString)
{
	// Sans la surcharge const char *, "alice" deviendrait… true.
	EXPECT_EQ(pg::toText("alice"), "alice");
}

TEST(Convert, EmptyOptionalIsNull)
{
	EXPECT_EQ(pg::toText(std::optional<int>{}), std::nullopt);
	EXPECT_EQ(pg::toText(std::optional<int>{7}), "7");
}

TEST(Convert, VectorToArrayLiteral)
{
	EXPECT_EQ(pg::toText(std::vector<int>{1, 2, 3}), R"({"1","2","3"})");
	const std::vector<std::optional<std::string>> tags{"a b", R"(c"d)", std::nullopt};
	EXPECT_EQ(pg::toText(tags), R"({"a b","c\"d",NULL})");
}

TEST(Convert, TimestampToIso8601)
{
	const pg::Timestamp t = sys_days{2026y / October / 27} + 8h + 30min + 123456us;
	EXPECT_EQ(pg::toText(t), "2026-10-27T08:30:00.123456Z");
}

TEST(Convert, ReadsTimestampWithOffset)
{
	pg::Timestamp t;
	ASSERT_TRUE(pg::fromText("2026-10-27 10:30:00.5+02", t));
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min + 500ms);
	ASSERT_TRUE(pg::fromText("2026-10-27 08:30:00", t));		// TIMESTAMP, sans décalage
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min);
	ASSERT_TRUE(pg::fromText("2026-10-27 13:00:00+04:30", t));
	EXPECT_EQ(t, sys_days{2026y / October / 27} + 8h + 30min);
}

TEST(Convert, RejectsMalformedText)
{
	pg::Timestamp t;
	EXPECT_FALSE(pg::fromText("2026-10-27", t));
	EXPECT_FALSE(pg::fromText("2026-13-01 00:00:00", t));
	EXPECT_FALSE(pg::fromText("infinity", t));
	int n = 0;
	EXPECT_FALSE(pg::fromText("12abc", n));
	bool b = false;
	EXPECT_FALSE(pg::fromText("true", b));		// PostgreSQL répond "t" ou "f"
}