#pragma once

#include <auth/session/ISessionStore.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace auth
{
	class SessionManager
	{
		public:
			explicit SessionManager(ISessionStore &store);

			bool createSession(
				const std::string &token,
				const std::string &subject,
				const std::chrono::system_clock::time_point &expiresAt
			);

			bool createSession(
				const std::string &token,
				const std::string &subject,
				std::chrono::seconds lifetime,
				const std::chrono::system_clock::time_point &now =
					std::chrono::system_clock::now()
			);

			std::optional<SessionRecord> findSession(
				const std::string &token
			) const;

			std::optional<SessionRecord> findByToken(
				const std::string &token
			) const;

			bool revokeSession(const std::string &token);
			bool revokeByToken(const std::string &token);

			bool isValid(
				const std::string &token,
				const std::chrono::system_clock::time_point &now =
					std::chrono::system_clock::now()
			) const;

			bool isExpired(
				const std::string &token,
				const std::chrono::system_clock::time_point &now =
					std::chrono::system_clock::now()
			) const;

		private:
			static std::string hashToken(const std::string &token);

			ISessionStore &_store;
	};
}
