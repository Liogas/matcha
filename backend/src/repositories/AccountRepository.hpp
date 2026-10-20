#pragma once

#include <auth/credentials/IUserLookupRepository.hpp>

#include <cstdint>
#include <chrono>
#include <string>

class Database;

struct AccountCreationResult
{
	enum class Status
	{
		Success,
		IdentifierAlreadyExists,
		DatabaseError
	};

	Status status;
	std::int64_t accountId;
};

struct EmailVerificationResult
{
	enum class Status
	{
		Verified,
		InvalidOrExpired,
		DatabaseError
	};

	Status status;
};

struct ResendVerificationResult
{
	enum class Status
	{
		Issued,
		Suppressed,
		DatabaseError
	};

	Status status;
	std::string email;
};

struct SessionResult
{
	enum class Status
	{
		Success,
		InvalidToken,
		DatabaseError
	};

	Status status;
	std::int64_t accountId;
	std::string email;
};

struct PasswordResetRequestResult
{
	enum class Status
	{
		Issued,
		Suppressed,
		DatabaseError
	};

	Status status;
	std::string email;
};

struct PasswordResetResult
{
	enum class Status
	{
		Reset,
		InvalidOrExpired,
		DatabaseError
	};

	Status status;
};

class AccountRepository : public auth::IUserLookupRepository
{
	public:
		explicit AccountRepository(Database &database);

		AccountCreationResult createPendingAccountAndProfile(
			const std::string &email,
			const std::string &passwordHash,
			const std::string &username,
			const std::string &firstName,
			const std::string &lastName,
			const std::string &verificationTokenHash,
			std::chrono::seconds verificationLifetime
		);
		EmailVerificationResult verifyEmailToken(
			const std::string &tokenHash
		);
		ResendVerificationResult issueEmailVerificationToken(
			const std::string &email,
			const std::string &tokenHash,
			std::chrono::seconds tokenLifetime
		);
		SessionResult createSession(
			std::int64_t accountId,
			const std::string &refreshTokenHash,
			std::chrono::seconds idleLifetime,
			std::chrono::seconds absoluteLifetime
		);
		SessionResult rotateRefreshToken(
			const std::string &currentTokenHash,
			const std::string &replacementTokenHash,
			std::chrono::seconds idleLifetime
		);
		bool revokeSessionByRefreshToken(
			const std::string &refreshTokenHash
		);
		bool revokeAllSessions(std::int64_t accountId);
		PasswordResetRequestResult issuePasswordResetToken(
			const std::string &email,
			const std::string &tokenHash,
			std::chrono::seconds tokenLifetime
		);
		PasswordResetResult resetPassword(
			const std::string &tokenHash,
			const std::string &passwordHash
		);
		auth::UserLookupResult findByEmail(
			const std::string &email
		) override;
		auth::UserLookupResult findById(
			std::int64_t id
		) override;

	private:
		Database &_database;
};
