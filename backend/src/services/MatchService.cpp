#include "MatchService.hpp"

#include "../messaging/MessagingRepository.hpp"

namespace
{
	// Verrou consultatif propre à la paire {a, b}, quel que soit l'ordre :
	// LEAST/GREATEST donnent la même clé pour (a, b) et (b, a). Rendu tout seul
	// au COMMIT ou au ROLLBACK. C'est la clé que prend aussi
	// MessagingRepository::ensureDirectConversation : la même transaction peut
	// reprendre un verrou qu'elle tient déjà.
	core::Result<void> lockPair(pg::Transaction &tx, std::int64_t a, std::int64_t b)
	{
		const auto locked = tx.query(
			"SELECT pg_advisory_xact_lock(hashtextextended("
			"LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' || "
			"GREATEST($1::BIGINT, $2::BIGINT)::TEXT, 0))",
			a, b);
		if (!locked)
			return std::unexpected(locked.error());
		return {};
	}
}

MatchService::MatchService(pg::ConnectionPool &pool) : _pool(pool)
{
}

core::Result<LikeResult> MatchService::like(std::int64_t likerAccountId,
											std::int64_t likedAccountId)
{
	auto tx = _pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	if (const auto locked = lockPair(*tx, likerAccountId, likedAccountId); !locked)
		return std::unexpected(locked.error());

	LikeRepository likes(*tx);
	const auto result = likes.like(likerAccountId, likedAccountId);
	if (!result)
		return std::unexpected(result.error());
	if (result->matched)
	{
		MessagingRepository messaging(*tx);
		if (!messaging.ensureDirectConversation(likerAccountId, likedAccountId))
			return core::fail(core::ErrorCode::Internal,
							  "cannot create the match conversation");
	}
	if (const auto committed = tx->commit(); !committed)
		return std::unexpected(committed.error());
	return result;
}

core::Result<void> MatchService::unlike(std::int64_t likerAccountId,
										std::int64_t likedAccountId)
{
	auto tx = _pool.begin();
	if (!tx)
		return std::unexpected(tx.error());
	if (const auto locked = lockPair(*tx, likerAccountId, likedAccountId); !locked)
		return std::unexpected(locked.error());

	LikeRepository likes(*tx);
	const auto removed = likes.unlike(likerAccountId, likedAccountId);
	if (!removed)
		return std::unexpected(removed.error());
	if (*removed)
	{
		MessagingRepository messaging(*tx);
		if (!messaging.removeDirectConversation(likerAccountId, likedAccountId))
			return core::fail(core::ErrorCode::Internal,
							  "cannot remove the match conversation");
	}
	return tx->commit();
}