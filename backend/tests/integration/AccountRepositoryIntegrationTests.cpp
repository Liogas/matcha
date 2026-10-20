#include <gtest/gtest.h>

#include "database/Database.hpp"
#include "repositories/AccountRepository.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <string>

namespace
{
	struct TestIdentity
	{
		std::string email;
		std::string username;
	};

	TestIdentity uniqueIdentity(const std::string &prefix)
	{
		const auto suffix = std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		);
		return {
			prefix + "-" + suffix + "@example.test",
			prefix + "_" + suffix
		};
	}

	std::string uniqueTokenHash()
	{
		std::ostringstream value;
		value << std::hex
			<< std::chrono::steady_clock::now().time_since_epoch().count();
		const std::string suffix = value.str();
		return std::string(64 - suffix.size(), '0') + suffix;
	}

	std::string uppercaseAscii(std::string value)
	{
		std::transform(
			value.begin(),
			value.end(),
			value.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::toupper(character));
			}
		);
		return value;
	}

	void deleteTestAccount(Database &database, const std::string &email)
	{
		database.executeParams(
			"DELETE FROM auth_accounts WHERE email = $1",
			{email}
		);
	}
}

TEST(AccountRepositoryIntegrationTest, CreatesAnUnverifiedAccountAndIncompleteProfile)
{
	Database database;
	AccountRepository repository(database);
	const auto identity = uniqueIdentity("pending");
	const std::string tokenHash = uniqueTokenHash();

	const auto result = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Test",
		"Account",
		tokenHash,
		std::chrono::hours{24}
	);

	ASSERT_EQ(result.status, AccountCreationResult::Status::Success);
	EXPECT_GT(result.accountId, 0);

	const auto query = database.executeParams(
		"SELECT a.email_verified_at, p.username, p.first_name, p.last_name, "
		"p.sexual_orientation, p.completed_at "
		"FROM auth_accounts a "
		"JOIN matcha_profiles p ON p.account_id = a.id "
		"WHERE a.id = $1",
		{std::to_string(result.accountId)}
	);
	ASSERT_FALSE(query.isError());
	ASSERT_EQ(query.rowCount(), 1);
	EXPECT_TRUE(query.isNull(0, 0));
	EXPECT_EQ(query.getString(0, 1).value(), identity.username);
	EXPECT_EQ(query.getString(0, 2).value(), "Test");
	EXPECT_EQ(query.getString(0, 3).value(), "Account");
	EXPECT_EQ(query.getString(0, 4).value(), "bisexual");
	EXPECT_TRUE(query.isNull(0, 5));

	const auto token = database.executeParams(
		"SELECT token_hash, expires_at > NOW() "
		"FROM email_verification_tokens WHERE account_id = $1",
		{std::to_string(result.accountId)}
	);
	ASSERT_FALSE(token.isError());
	ASSERT_EQ(token.rowCount(), 1);
	EXPECT_EQ(token.getString(0, 0).value(), tokenHash);
	EXPECT_TRUE(token.getBool(0, 1).value());

	deleteTestAccount(database, identity.email);
}

TEST(AccountRepositoryIntegrationTest, VerifiesEmailTokenOnlyOnce)
{
	Database database;
	AccountRepository repository(database);
	const auto identity = uniqueIdentity("verify");
	const std::string tokenHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Verify",
		"Account",
		tokenHash,
		std::chrono::hours{24}
	);
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);

	const auto pendingLookup = repository.findByEmail(identity.email);
	EXPECT_EQ(
		pendingLookup.status,
		auth::UserLookupResult::Status::NotFound
	);
	EXPECT_EQ(
		repository.findById(created.accountId).status,
		auth::UserLookupResult::Status::NotFound
	);

	const auto verified = repository.verifyEmailToken(tokenHash);
	EXPECT_EQ(verified.status, EmailVerificationResult::Status::Verified);
	const auto verifiedLookup = repository.findByEmail(identity.email);
	ASSERT_EQ(
		verifiedLookup.status,
		auth::UserLookupResult::Status::Found
	);
	ASSERT_TRUE(verifiedLookup.user.has_value());
	EXPECT_EQ(verifiedLookup.user->id, created.accountId);
	const auto repeated = repository.verifyEmailToken(tokenHash);
	EXPECT_EQ(
		repeated.status,
		EmailVerificationResult::Status::InvalidOrExpired
	);

	const auto account = database.executeParams(
		"SELECT email_verified_at IS NOT NULL "
		"FROM auth_accounts WHERE id = $1",
		{std::to_string(created.accountId)}
	);
	ASSERT_FALSE(account.isError());
	ASSERT_EQ(account.rowCount(), 1);
	EXPECT_TRUE(account.getBool(0, 0).value());

	deleteTestAccount(database, identity.email);
}

TEST(AccountRepositoryIntegrationTest, ResendsVerificationAfterCooldownAndRotatesToken)
{
	Database database;
	AccountRepository repository(database);
	const auto identity = uniqueIdentity("resend");
	const std::string firstTokenHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Resend",
		"Account",
		firstTokenHash,
		std::chrono::hours{24}
	);
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);

	const std::string replacementTokenHash = uniqueTokenHash();
	const auto tooSoon = repository.issueEmailVerificationToken(
		identity.email,
		replacementTokenHash,
		std::chrono::hours{24}
	);
	EXPECT_EQ(
		tooSoon.status,
		ResendVerificationResult::Status::Suppressed
	);

	const auto resetCooldown = database.executeParams(
		"UPDATE auth_accounts SET email_verification_last_sent_at = NULL "
		"WHERE id = $1",
		{std::to_string(created.accountId)}
	);
	ASSERT_FALSE(resetCooldown.isError());

	const auto resent = repository.issueEmailVerificationToken(
		identity.email,
		replacementTokenHash,
		std::chrono::hours{24}
	);
	ASSERT_EQ(resent.status, ResendVerificationResult::Status::Issued);
	EXPECT_EQ(resent.email, identity.email);
	EXPECT_EQ(
		repository.verifyEmailToken(firstTokenHash).status,
		EmailVerificationResult::Status::InvalidOrExpired
	);
	EXPECT_EQ(
		repository.verifyEmailToken(replacementTokenHash).status,
		EmailVerificationResult::Status::Verified
	);
	EXPECT_EQ(
		repository.issueEmailVerificationToken(
			identity.email,
			uniqueTokenHash(),
			std::chrono::hours{24}
		).status,
		ResendVerificationResult::Status::Suppressed
	);

	deleteTestAccount(database, identity.email);
}

TEST(AccountRepositoryIntegrationTest, RotatesRefreshTokenAndRevokesSessionOnReplay)
{
	Database database;
	AccountRepository repository(database);
	const auto identity = uniqueIdentity("session");
	const std::string verificationHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Session",
		"Account",
		verificationHash,
		std::chrono::hours{24}
	);
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);
	ASSERT_EQ(
		repository.verifyEmailToken(verificationHash).status,
		EmailVerificationResult::Status::Verified
	);

	const std::string firstRefreshHash = uniqueTokenHash();
	const auto session = repository.createSession(
		created.accountId,
		firstRefreshHash,
		std::chrono::days{7},
		std::chrono::days{30}
	);
	ASSERT_EQ(session.status, SessionResult::Status::Success);
	EXPECT_EQ(session.accountId, created.accountId);
	EXPECT_EQ(session.email, identity.email);

	const std::string secondRefreshHash = uniqueTokenHash();
	const auto rotated = repository.rotateRefreshToken(
		firstRefreshHash,
		secondRefreshHash,
		std::chrono::days{7}
	);
	ASSERT_EQ(rotated.status, SessionResult::Status::Success);
	EXPECT_EQ(rotated.accountId, created.accountId);
	EXPECT_EQ(rotated.email, identity.email);

	const auto replay = repository.rotateRefreshToken(
		firstRefreshHash,
		uniqueTokenHash(),
		std::chrono::days{7}
	);
	EXPECT_EQ(replay.status, SessionResult::Status::InvalidToken);
	EXPECT_EQ(
		repository.rotateRefreshToken(
			secondRefreshHash,
			uniqueTokenHash(),
			std::chrono::days{7}
		).status,
		SessionResult::Status::InvalidToken
	);

	deleteTestAccount(database, identity.email);
}

TEST(AccountRepositoryIntegrationTest, PasswordResetConsumesTokenAndRevokesSessions)
{
	Database database;
	AccountRepository repository(database);
	const auto identity = uniqueIdentity("passwordreset");
	const std::string verificationHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"old-password-hash",
		identity.username,
		"Password",
		"Reset",
		verificationHash,
		std::chrono::hours{24}
	);
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);
	ASSERT_EQ(
		repository.verifyEmailToken(verificationHash).status,
		EmailVerificationResult::Status::Verified
	);

	const std::string refreshHash = uniqueTokenHash();
	ASSERT_EQ(
		repository.createSession(
			created.accountId,
			refreshHash,
			std::chrono::days{7},
			std::chrono::days{30}
		).status,
		SessionResult::Status::Success
	);
	const std::string resetHash = uniqueTokenHash();
	const auto issued = repository.issuePasswordResetToken(
		identity.email,
		resetHash,
		std::chrono::minutes{30}
	);
	ASSERT_EQ(
		issued.status,
		PasswordResetRequestResult::Status::Issued
	);
	EXPECT_EQ(issued.email, identity.email);
	EXPECT_EQ(
		repository.issuePasswordResetToken(
			identity.email,
			uniqueTokenHash(),
			std::chrono::minutes{30}
		).status,
		PasswordResetRequestResult::Status::Suppressed
	);

	const auto reset = repository.resetPassword(
		resetHash,
		"new-password-hash"
	);
	EXPECT_EQ(reset.status, PasswordResetResult::Status::Reset);
	EXPECT_EQ(
		repository.resetPassword(
			resetHash,
			"another-password-hash"
		).status,
		PasswordResetResult::Status::InvalidOrExpired
	);
	EXPECT_EQ(
		repository.rotateRefreshToken(
			refreshHash,
			uniqueTokenHash(),
			std::chrono::days{7}
		).status,
		SessionResult::Status::InvalidToken
	);

	const auto password = database.executeParams(
		"SELECT password_hash FROM auth_accounts WHERE id = $1",
		{std::to_string(created.accountId)}
	);
	ASSERT_FALSE(password.isError());
	ASSERT_EQ(password.rowCount(), 1);
	EXPECT_EQ(password.getString(0, 0).value(), "new-password-hash");

	deleteTestAccount(database, identity.email);
}

TEST(AccountRepositoryIntegrationTest, RejectsDuplicateEmailOrUsername)
{
	Database database;
	AccountRepository repository(database);
	const auto first = uniqueIdentity("first");
	const auto second = uniqueIdentity("second");

	const auto firstResult = repository.createPendingAccountAndProfile(
		first.email,
		"test-password-hash",
		first.username,
		"First",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24}
	);
	ASSERT_EQ(firstResult.status, AccountCreationResult::Status::Success);

	const auto duplicateEmail = repository.createPendingAccountAndProfile(
		uppercaseAscii(first.email),
		"test-password-hash",
		second.username,
		"Second",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24}
	);
	EXPECT_EQ(
		duplicateEmail.status,
		AccountCreationResult::Status::IdentifierAlreadyExists
	);

	const auto duplicateUsername = repository.createPendingAccountAndProfile(
		second.email,
		"test-password-hash",
		uppercaseAscii(first.username),
		"Second",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24}
	);
	EXPECT_EQ(
		duplicateUsername.status,
		AccountCreationResult::Status::IdentifierAlreadyExists
	);
	const auto orphanCheck = database.executeParams(
		"SELECT COUNT(*) FROM auth_accounts WHERE email = $1",
		{second.email}
	);
	ASSERT_FALSE(orphanCheck.isError());
	ASSERT_EQ(orphanCheck.rowCount(), 1);
	EXPECT_EQ(orphanCheck.getInt64(0, 0).value(), 0);

	deleteTestAccount(database, first.email);
	deleteTestAccount(database, second.email);
}
