#pragma once

#include <auth/session/SessionRecord.hpp>

#include <optional>
#include <string>

namespace auth
{
	class ISessionStore
	{
		public:
			virtual ~ISessionStore() = default;

			virtual bool create(const SessionRecord &record) = 0;
			virtual std::optional<SessionRecord> findByTokenHash(
				const std::string &tokenHash
			) = 0;
			virtual bool revokeByTokenHash(
				const std::string &tokenHash
			) = 0;
	};
}
