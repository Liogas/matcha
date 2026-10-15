#pragma once

#include <auth/session/ISessionStore.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>

namespace auth
{
	class InMemorySessionStore : public ISessionStore
	{
		public:
			bool create(const SessionRecord &record) override;
			std::optional<SessionRecord> findByTokenHash(
				const std::string &tokenHash
			) override;
			bool revokeByTokenHash(
				const std::string &tokenHash
			) override;

		private:
			std::unordered_map<std::string, SessionRecord> _sessions;
			std::mutex _mutex;
			std::chrono::system_clock::time_point _nextCleanupAt{};
	};
}
