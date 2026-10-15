#include <auth/session/SessionManager.hpp>

#include <iomanip>
#include <openssl/sha.h>
#include <sstream>

namespace auth
{
	SessionManager::SessionManager(ISessionStore &store):
		_store(store)
	{}

	std::string SessionManager::hashToken(const std::string &token)
	{
		unsigned char digest[SHA256_DIGEST_LENGTH];
		SHA256(
			reinterpret_cast<const unsigned char *>(token.data()),
			token.size(),
			digest
		);

		std::ostringstream oss;
		oss << std::hex << std::nouppercase;
		for (const unsigned char byte : digest)
		{
			oss << std::setw(2) << std::setfill('0')
				<< static_cast<int>(byte);
		}
		return oss.str();
	}

	bool SessionManager::createSession(
		const std::string &token,
		const std::string &subject,
		const std::chrono::system_clock::time_point &expiresAt
	)
	{
		if (token.empty() || subject.empty())
			return false;
		return this->_store.create({
			hashToken(token),
			subject,
			expiresAt
		});
	}

	bool SessionManager::createSession(
		const std::string &token,
		const std::string &subject,
		std::chrono::seconds lifetime,
		const std::chrono::system_clock::time_point &now
	)
	{
		if (lifetime <= std::chrono::seconds::zero())
			return false;
		return this->createSession(
			token,
			subject,
			now + lifetime
		);
	}

	std::optional<SessionRecord> SessionManager::findSession(
		const std::string &token
	) const
	{
		if (token.empty())
			return std::nullopt;
		return this->_store.findByTokenHash(hashToken(token));
	}

	std::optional<SessionRecord> SessionManager::findByToken(
		const std::string &token
	) const
	{
		return this->findSession(token);
	}

	bool SessionManager::revokeSession(const std::string &token)
	{
		if (token.empty())
			return false;
		return this->_store.revokeByTokenHash(hashToken(token));
	}

	bool SessionManager::revokeByToken(const std::string &token)
	{
		return this->revokeSession(token);
	}

	bool SessionManager::isValid(
		const std::string &token,
		const std::chrono::system_clock::time_point &now
	) const
	{
		const auto session = this->findSession(token);
		if (!session.has_value())
			return false;
		return session->expiresAt > now;
	}

	bool SessionManager::isExpired(
		const std::string &token,
		const std::chrono::system_clock::time_point &now
	) const
	{
		return !this->isValid(token, now);
	}
}
