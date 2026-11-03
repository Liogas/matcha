#pragma once

#include "../repositories/LikeRepository.hpp"

#include <core/Result.hpp>
#include <pg/Pool.hpp>

#include <cstdint>

// Like, match et conversation dans UNE transaction : tout est enregistré, ou
// rien. Un verrou par paire de comptes fait passer l'un après l'autre deux
// likes croisés envoyés au même instant.
class MatchService
{
public:
	explicit MatchService(pg::ConnectionPool &pool);

	core::Result<LikeResult> like(std::int64_t likerAccountId,
								  std::int64_t likedAccountId);
	core::Result<void> unlike(std::int64_t likerAccountId,
							  std::int64_t likedAccountId);

private:
	pg::ConnectionPool &_pool;
};