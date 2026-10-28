#include <pg/SqlState.hpp>
#include <gtest/gtest.h>

TEST(SqlState, UniqueViolationIsConflict)
{
	EXPECT_EQ(pg::errorCodeFor("23505"), core::ErrorCode::Conflict);
}

TEST(SqlState, BadDataIsInvalid)
{
	EXPECT_EQ(pg::errorCodeFor("23503"), core::ErrorCode::Invalid);	// clé étrangère
	EXPECT_EQ(pg::errorCodeFor("23514"), core::ErrorCode::Invalid);	// CHECK
	EXPECT_EQ(pg::errorCodeFor("22P02"), core::ErrorCode::Invalid);	// 'abc' en entier
}

TEST(SqlState, RetryableIsUnavailable)
{
	EXPECT_EQ(pg::errorCodeFor("40001"), core::ErrorCode::Unavailable);
	EXPECT_EQ(pg::errorCodeFor("40P01"), core::ErrorCode::Unavailable);
	EXPECT_EQ(pg::errorCodeFor("08006"), core::ErrorCode::Unavailable);
}

TEST(SqlState, AnythingElseIsInternal)
{
	EXPECT_EQ(pg::errorCodeFor("42601"), core::ErrorCode::Internal);	// erreur de syntaxe
	EXPECT_EQ(pg::errorCodeFor("42P01"), core::ErrorCode::Internal);	// table inconnue
}