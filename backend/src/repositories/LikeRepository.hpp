#pragma once

#include <cstdint>
#include <optional>
#include <vector>

class Database;

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
		explicit LikeRepository(Database &database);

		LikeResult like(std::int64_t likerAccountId,
			std::int64_t likedAccountId);
		bool unlike(std::int64_t likerAccountId,
			std::int64_t likedAccountId);
		std::vector<std::int64_t> listLikedAccountIds(
			std::int64_t likerAccountId);
		std::vector<std::int64_t> listIncomingLikeAccountIds(
			std::int64_t likedAccountId);
		std::vector<MatchRecord> listMatches(std::int64_t accountId);
		std::vector<std::pair<std::int64_t, std::int64_t>> listAllMatches();

	private:
		Database &_database;
};
