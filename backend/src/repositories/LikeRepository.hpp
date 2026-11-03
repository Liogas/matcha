#pragma once

#include <core/Result.hpp>
#include <pg/Executor.hpp>

#include <cstdint>
#include <optional>
#include <vector>

struct LikeResult
{
	bool liked;
	bool matched;
	std::optional<std::int64_t> matchId;
};

struct MatchRecord
{
	std::int64_t id;
	std::int64_t otherAccountId;
};

class LikeRepository
{
public:
	explicit LikeRepository(pg::Executor &database);

	// À appeler dans la transaction de MatchService : seul, un like
	// simultané dans l'autre sens peut faire rater le match.
	core::Result<LikeResult> like(std::int64_t likerAccountId,
								  std::int64_t likedAccountId);
	// true si un like a été supprimé.
	core::Result<bool> unlike(std::int64_t likerAccountId,
							  std::int64_t likedAccountId);
	std::vector<std::int64_t> listLikedAccountIds(
		std::int64_t likerAccountId);
	std::vector<std::int64_t> listIncomingLikeAccountIds(
		std::int64_t likedAccountId);
	std::vector<MatchRecord> listMatches(std::int64_t accountId);

private:
	pg::Executor &_database;
};