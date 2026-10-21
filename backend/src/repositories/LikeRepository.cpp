#include "LikeRepository.hpp"

#include "../database/Database.hpp"

#include <vector>
#include <stdexcept>

LikeRepository::LikeRepository(Database &database):
	_database(database)
{
}

LikeResult LikeRepository::like(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	const std::vector<std::string> params = {
		std::to_string(likerAccountId), std::to_string(likedAccountId)
	};
	const auto result = _database.executeParams(R"(
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
	)", params);
	if (result.isError())
		throw std::runtime_error("Unable to create profile like");
	if (result.rowCount() == 0)
		return {true, false, std::nullopt};
	const auto matchId = result.getInt64(0, 0);
	if (!matchId)
		throw std::runtime_error("Invalid match returned by database");
	return {true, true, matchId};
}

std::vector<std::int64_t> LikeRepository::listIncomingLikeAccountIds(
	std::int64_t likedAccountId)
{
	const auto result = _database.executeParams(
		"SELECT liker_account_id FROM profile_likes "
		"WHERE liked_account_id = $1 ORDER BY created_at DESC",
		{std::to_string(likedAccountId)});
	if (result.isError())
		throw std::runtime_error("Unable to list incoming profile likes");
	std::vector<std::int64_t> ids;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto id = result.getInt64(row, 0);
		if (id)
			ids.push_back(*id);
	}
	return ids;
}

bool LikeRepository::unlike(std::int64_t likerAccountId,
	std::int64_t likedAccountId)
{
	const auto result = _database.executeParams(R"(
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
		SELECT EXISTS (SELECT 1 FROM deleted_like)
	)",
		{std::to_string(likerAccountId), std::to_string(likedAccountId)});
	if (result.isError())
		throw std::runtime_error("Unable to remove profile like");
	const auto removed = result.getBool(0, 0);
	if (!removed)
		throw std::runtime_error("Invalid like removal result");
	return *removed;
}

std::vector<std::int64_t> LikeRepository::listLikedAccountIds(
	std::int64_t likerAccountId)
{
	const auto result = _database.executeParams(
		"SELECT liked_account_id FROM profile_likes "
		"WHERE liker_account_id = $1 ORDER BY created_at DESC",
		{std::to_string(likerAccountId)});
	if (result.isError())
		throw std::runtime_error("Unable to list profile likes");
	std::vector<std::int64_t> ids;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto id = result.getInt64(row, 0);
		if (id)
			ids.push_back(*id);
	}
	return ids;
}

std::vector<MatchRecord> LikeRepository::listMatches(std::int64_t accountId)
{
	const auto result = _database.executeParams(R"(
		SELECT id,
			CASE WHEN first_account_id = $1
				THEN second_account_id ELSE first_account_id END
		FROM profile_matches
		WHERE first_account_id = $1 OR second_account_id = $1
		ORDER BY created_at DESC
	)", {std::to_string(accountId)});
	if (result.isError())
		throw std::runtime_error("Unable to list profile matches");
	std::vector<MatchRecord> matches;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto id = result.getInt64(row, 0);
		const auto otherId = result.getInt64(row, 1);
		if (id && otherId)
			matches.push_back({*id, *otherId});
	}
	return matches;
}

std::vector<std::pair<std::int64_t, std::int64_t>>
LikeRepository::listAllMatches()
{
	const auto result = _database.execute(
		"SELECT first_account_id, second_account_id "
		"FROM profile_matches ORDER BY id");
	if (result.isError())
		throw std::runtime_error("Unable to list all profile matches");
	std::vector<std::pair<std::int64_t, std::int64_t>> matches;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto first = result.getInt64(row, 0);
		const auto second = result.getInt64(row, 1);
		if (first && second)
			matches.emplace_back(*first, *second);
	}
	return matches;
}
