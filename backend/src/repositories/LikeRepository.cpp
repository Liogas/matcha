#include "LikeRepository.hpp"

#include <stdexcept>

namespace
{
	std::vector<std::int64_t> accountIds(const pg::Rows &rows, const char *column)
	{
		std::vector<std::int64_t> ids;
		for (const pg::Row row : rows)
			ids.push_back(row.get<std::int64_t>(column));
		return ids;
	}
}

LikeRepository::LikeRepository(pg::Executor &database) : _database(database)
{
}

core::Result<LikeResult> LikeRepository::like(std::int64_t likerAccountId,
											  std::int64_t likedAccountId)
{
	const auto result = _database.query(R"(
        WITH inserted_like AS (
            INSERT INTO profile_likes (liker_account_id, liked_account_id)
            VALUES ($1, $2)
            ON CONFLICT DO NOTHING
        ), ensured_match AS (
            INSERT INTO profile_matches (first_account_id, second_account_id)
            SELECT LEAST($1::BIGINT, $2::BIGINT),
                GREATEST($1::BIGINT, $2::BIGINT)
            WHERE EXISTS (
                SELECT 1 FROM profile_likes
                WHERE liker_account_id = $2 AND liked_account_id = $1
            )
            ON CONFLICT (first_account_id, second_account_id)
            DO UPDATE SET created_at = profile_matches.created_at
            RETURNING id
        )
        SELECT id FROM ensured_match
    )",
										likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	if (result->empty())
		return LikeResult{true, false, std::nullopt};
	return LikeResult{true, true, (*result)[0].get<std::int64_t>("id")};
}

std::vector<std::int64_t> LikeRepository::listIncomingLikeAccountIds(
	std::int64_t likedAccountId)
{
	const auto result = _database.query(
		"SELECT liker_account_id FROM profile_likes "
		"WHERE liked_account_id = $1 ORDER BY created_at DESC",
		likedAccountId);
	if (!result)
		throw std::runtime_error("Unable to list incoming profile likes");
	return accountIds(*result, "liker_account_id");
}

core::Result<bool> LikeRepository::unlike(std::int64_t likerAccountId,
										  std::int64_t likedAccountId)
{
	const auto result = _database.query(R"(
        WITH deleted_like AS (
            DELETE FROM profile_likes
            WHERE liker_account_id = $1 AND liked_account_id = $2
            RETURNING 1
        ), deleted_match AS (
            DELETE FROM profile_matches
            WHERE first_account_id = LEAST($1::BIGINT, $2::BIGINT)
                AND second_account_id = GREATEST($1::BIGINT, $2::BIGINT)
                AND EXISTS (SELECT 1 FROM deleted_like)
            RETURNING 1
        )
        SELECT EXISTS (SELECT 1 FROM deleted_like) AS removed
    )",
										likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	return (*result)[0].get<bool>("removed");
}

std::vector<std::int64_t> LikeRepository::listLikedAccountIds(
	std::int64_t likerAccountId)
{
	const auto result = _database.query(
		"SELECT liked_account_id FROM profile_likes "
		"WHERE liker_account_id = $1 ORDER BY created_at DESC",
		likerAccountId);
	if (!result)
		throw std::runtime_error("Unable to list profile likes");
	return accountIds(*result, "liked_account_id");
}

std::vector<MatchRecord> LikeRepository::listMatches(std::int64_t accountId)
{
	const auto result = _database.query(R"(
        SELECT id,
            CASE WHEN first_account_id = $1
                THEN second_account_id ELSE first_account_id END
                AS other_account_id
        FROM profile_matches
        WHERE first_account_id = $1 OR second_account_id = $1
        ORDER BY created_at DESC
    )",
										accountId);
	if (!result)
		throw std::runtime_error("Unable to list profile matches");
	std::vector<MatchRecord> matches;
	for (const pg::Row row : *result)
		matches.push_back({row.get<std::int64_t>("id"),
						   row.get<std::int64_t>("other_account_id")});
	return matches;
}