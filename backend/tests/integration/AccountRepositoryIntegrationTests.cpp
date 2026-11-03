#include <gtest/gtest.h>

#include "repositories/AccountRepository.hpp"
#include "support/DatabaseTest.hpp"

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
			std::chrono::steady_clock::now().time_since_epoch().count());
		return {
			prefix + "-" + suffix + "@example.test",
			prefix + "_" + suffix};
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
			});
		return value;
	}
}

// Chaque test tourne dans une transaction annulée à la fin (RepositoryTest) :
// plus besoin de supprimer les comptes créés.
class AccountRepositoryIntegrationTest : public RepositoryTest
{
};

TEST_F(AccountRepositoryIntegrationTest, CreatesAnUnverifiedAccountAndIncompleteProfile)
{
	AccountRepository repository(this->database());
	const auto identity = uniqueIdentity("pending");
	const std::string tokenHash = uniqueTokenHash();

	const auto result = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Test",
		"Account",
		tokenHash,
		std::chrono::hours{24});

	ASSERT_EQ(result.status, AccountCreationResult::Status::Success);
	EXPECT_GT(result.accountId, 0);

	const auto query = this->database().query(
		"SELECT a.email_verified_at, p.username, p.first_name, p.last_name, "
		"p.sexual_orientation, p.completed_at "
		"FROM auth_accounts a "
		"JOIN matcha_profiles p ON p.account_id = a.id "
		"WHERE a.id = $1",
		result.accountId);
	ASSERT_TRUE(query) << query.error().message;
	ASSERT_EQ(query->size(), 1u);
	const pg::Row account = (*query)[0];
	EXPECT_FALSE(account.get<std::optional<pg::Timestamp>>("email_verified_at"));
	EXPECT_EQ(account.get<std::string>("username"), identity.username);
	EXPECT_EQ(account.get<std::string>("first_name"), "Test");
	EXPECT_EQ(account.get<std::string>("last_name"), "Account");
	EXPECT_EQ(account.get<std::string>("sexual_orientation"), "bisexual");
	EXPECT_FALSE(account.get<std::optional<pg::Timestamp>>("completed_at"));

	const auto token = this->database().query(
		"SELECT token_hash, expires_at > NOW() AS valid "
		"FROM email_verification_tokens WHERE account_id = $1",
		result.accountId);
	ASSERT_TRUE(token) << token.error().message;
	ASSERT_EQ(token->size(), 1u);
	EXPECT_EQ((*token)[0].get<std::string>("token_hash"), tokenHash);
	EXPECT_TRUE((*token)[0].get<bool>("valid"));
}

TEST_F(AccountRepositoryIntegrationTest, VerifiesEmailTokenOnlyOnce)
{
	AccountRepository repository(this->database());
	const auto identity = uniqueIdentity("verify");
	const std::string tokenHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Verify",
		"Account",
		tokenHash,
		std::chrono::hours{24});
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);

	const auto pendingLookup = repository.findByEmail(identity.email);
	EXPECT_EQ(
		pendingLookup.status,
		auth::UserLookupResult::Status::NotFound);
	EXPECT_EQ(
		repository.findById(created.accountId).status,
		auth::UserLookupResult::Status::NotFound);

	const auto verified = repository.verifyEmailToken(tokenHash);
	EXPECT_EQ(verified.status, EmailVerificationResult::Status::Verified);
	const auto verifiedLookup = repository.findByEmail(identity.email);
	ASSERT_EQ(
		verifiedLookup.status,
		auth::UserLookupResult::Status::Found);
	ASSERT_TRUE(verifiedLookup.user.has_value());
	EXPECT_EQ(verifiedLookup.user->id, created.accountId);
	const auto repeated = repository.verifyEmailToken(tokenHash);
	EXPECT_EQ(
		repeated.status,
		EmailVerificationResult::Status::InvalidOrExpired);

	const auto account = this->database().query(
		"SELECT email_verified_at IS NOT NULL AS verified "
		"FROM auth_accounts WHERE id = $1",
		created.accountId);
	ASSERT_TRUE(account) << account.error().message;
	ASSERT_EQ(account->size(), 1u);
	EXPECT_TRUE((*account)[0].get<bool>("verified"));
}

TEST_F(AccountRepositoryIntegrationTest, ResendsVerificationAfterCooldownAndRotatesToken)
{
	AccountRepository repository(this->database());
	const auto identity = uniqueIdentity("resend");
	const std::string firstTokenHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Resend",
		"Account",
		firstTokenHash,
		std::chrono::hours{24});
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);

	const std::string replacementTokenHash = uniqueTokenHash();
	const auto tooSoon = repository.issueEmailVerificationToken(
		identity.email,
		replacementTokenHash,
		std::chrono::hours{24});
	EXPECT_EQ(
		tooSoon.status,
		ResendVerificationResult::Status::Suppressed);

	const auto resetCooldown = this->database().query(
		"UPDATE auth_accounts SET email_verification_last_sent_at = NULL "
		"WHERE id = $1",
		created.accountId);
	ASSERT_TRUE(resetCooldown) << resetCooldown.error().message;

	const auto resent = repository.issueEmailVerificationToken(
		identity.email,
		replacementTokenHash,
		std::chrono::hours{24});
	ASSERT_EQ(resent.status, ResendVerificationResult::Status::Issued);
	EXPECT_EQ(resent.email, identity.email);
	EXPECT_EQ(
		repository.verifyEmailToken(firstTokenHash).status,
		EmailVerificationResult::Status::InvalidOrExpired);
	EXPECT_EQ(
		repository.verifyEmailToken(replacementTokenHash).status,
		EmailVerificationResult::Status::Verified);
	EXPECT_EQ(
		repository.issueEmailVerificationToken(
					  identity.email,
					  uniqueTokenHash(),
					  std::chrono::hours{24})
			.status,
		ResendVerificationResult::Status::Suppressed);
}

TEST_F(AccountRepositoryIntegrationTest, RotatesRefreshTokenAndRevokesSessionOnReplay)
{
	AccountRepository repository(this->database());
	const auto identity = uniqueIdentity("session");
	const std::string verificationHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"test-password-hash",
		identity.username,
		"Session",
		"Account",
		verificationHash,
		std::chrono::hours{24});
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);
	ASSERT_EQ(
		repository.verifyEmailToken(verificationHash).status,
		EmailVerificationResult::Status::Verified);

	const std::string firstRefreshHash = uniqueTokenHash();
	const auto session = repository.createSession(
		created.accountId,
		firstRefreshHash,
		std::chrono::days{7},
		std::chrono::days{30});
	ASSERT_EQ(session.status, SessionResult::Status::Success);
	EXPECT_EQ(session.accountId, created.accountId);
	EXPECT_EQ(session.email, identity.email);

	const std::string secondRefreshHash = uniqueTokenHash();
	const auto rotated = repository.rotateRefreshToken(
		firstRefreshHash,
		secondRefreshHash,
		std::chrono::days{7});
	ASSERT_EQ(rotated.status, SessionResult::Status::Success);
	EXPECT_EQ(rotated.accountId, created.accountId);
	EXPECT_EQ(rotated.email, identity.email);

	const auto replay = repository.rotateRefreshToken(
		firstRefreshHash,
		uniqueTokenHash(),
		std::chrono::days{7});
	EXPECT_EQ(replay.status, SessionResult::Status::InvalidToken);
	EXPECT_EQ(
		repository.rotateRefreshToken(
					  secondRefreshHash,
					  uniqueTokenHash(),
					  std::chrono::days{7})
			.status,
		SessionResult::Status::InvalidToken);
}

TEST_F(AccountRepositoryIntegrationTest, PasswordResetConsumesTokenAndRevokesSessions)
{
	AccountRepository repository(this->database());
	const auto identity = uniqueIdentity("passwordreset");
	const std::string verificationHash = uniqueTokenHash();
	const auto created = repository.createPendingAccountAndProfile(
		identity.email,
		"old-password-hash",
		identity.username,
		"Password",
		"Reset",
		verificationHash,
		std::chrono::hours{24});
	ASSERT_EQ(created.status, AccountCreationResult::Status::Success);
	ASSERT_EQ(
		repository.verifyEmailToken(verificationHash).status,
		EmailVerificationResult::Status::Verified);

	const std::string refreshHash = uniqueTokenHash();
	ASSERT_EQ(
		repository.createSession(
					  created.accountId,
					  refreshHash,
					  std::chrono::days{7},
					  std::chrono::days{30})
			.status,
		SessionResult::Status::Success);
	const std::string resetHash = uniqueTokenHash();
	const auto issued = repository.issuePasswordResetToken(
		identity.email,
		resetHash,
		std::chrono::minutes{30});
	ASSERT_EQ(
		issued.status,
		PasswordResetRequestResult::Status::Issued);
	EXPECT_EQ(issued.email, identity.email);
	EXPECT_EQ(
		repository.issuePasswordResetToken(
					  identity.email,
					  uniqueTokenHash(),
					  std::chrono::minutes{30})
			.status,
		PasswordResetRequestResult::Status::Suppressed);

	const auto reset = repository.resetPassword(
		resetHash,
		"new-password-hash");
	EXPECT_EQ(reset.status, PasswordResetResult::Status::Reset);
	EXPECT_EQ(
		repository.resetPassword(
					  resetHash,
					  "another-password-hash")
			.status,
		PasswordResetResult::Status::InvalidOrExpired);
	EXPECT_EQ(
		repository.rotateRefreshToken(
					  refreshHash,
					  uniqueTokenHash(),
					  std::chrono::days{7})
			.status,
		SessionResult::Status::InvalidToken);

	const auto password = this->database().query(
		"SELECT password_hash FROM auth_accounts WHERE id = $1",
		created.accountId);
	ASSERT_TRUE(password) << password.error().message;
	ASSERT_EQ(password->size(), 1u);
	EXPECT_EQ((*password)[0].get<std::string>("password_hash"), "new-password-hash");
}

TEST_F(AccountRepositoryIntegrationTest, RejectsDuplicateEmailOrUsername)
{
	AccountRepository repository(this->database());
	const auto first = uniqueIdentity("first");
	const auto second = uniqueIdentity("second");

	const auto firstResult = repository.createPendingAccountAndProfile(
		first.email,
		"test-password-hash",
		first.username,
		"First",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24});
	ASSERT_EQ(firstResult.status, AccountCreationResult::Status::Success);

	const auto duplicateEmail = repository.createPendingAccountAndProfile(
		uppercaseAscii(first.email),
		"test-password-hash",
		second.username,
		"Second",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24});
	EXPECT_EQ(
		duplicateEmail.status,
		AccountCreationResult::Status::IdentifierAlreadyExists);

	const auto duplicateUsername = repository.createPendingAccountAndProfile(
		second.email,
		"test-password-hash",
		uppercaseAscii(first.username),
		"Second",
		"Account",
		uniqueTokenHash(),
		std::chrono::hours{24});
	EXPECT_EQ(
		duplicateUsername.status,
		AccountCreationResult::Status::IdentifierAlreadyExists);
	const auto orphanCheck = this->database().query(
		"SELECT COUNT(*) AS n FROM auth_accounts WHERE email = $1",
		second.email);
	ASSERT_TRUE(orphanCheck) << orphanCheck.error().message;
	EXPECT_EQ((*orphanCheck)[0].get<std::int64_t>("n"), 0);
}
