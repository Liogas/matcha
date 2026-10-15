#include <auth/session/InMemorySessionStore.hpp>

#include <mutex>

namespace auth
{
	bool InMemorySessionStore::create(const SessionRecord &record)
	{
		const std::lock_guard<std::mutex> lock(this->_mutex);
		const auto now = std::chrono::system_clock::now();
		if (now >= this->_nextCleanupAt)
		{
			for (auto it = this->_sessions.begin(); it != this->_sessions.end();)
			{
				if (it->second.expiresAt <= now)
					it = this->_sessions.erase(it);
				else
					++it;
			}
			this->_nextCleanupAt = now + std::chrono::minutes{1};
		}
		if (record.tokenHash.empty() ||
			record.subject.empty() ||
			record.expiresAt <= now)
			return false;
		auto [it, inserted] = this->_sessions.emplace(
			record.tokenHash,
			record
		);
		return inserted;
	}

	std::optional<SessionRecord> InMemorySessionStore::findByTokenHash(
		const std::string &tokenHash
	)
	{
		const std::lock_guard<std::mutex> lock(this->_mutex);
		auto it = this->_sessions.find(tokenHash);
		if (it == this->_sessions.end())
			return std::nullopt;
		if (it->second.expiresAt <= std::chrono::system_clock::now())
		{
			this->_sessions.erase(it);
			return std::nullopt;
		}
		return it->second;
	}

	bool InMemorySessionStore::revokeByTokenHash(
		const std::string &tokenHash
	)
	{
		const std::lock_guard<std::mutex> lock(this->_mutex);
		return this->_sessions.erase(tokenHash) != 0;
	}
}
