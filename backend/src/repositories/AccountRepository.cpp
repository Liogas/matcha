#include "AccountRepository.hpp"

#include "../database/Database.hpp"

AccountRepository::AccountRepository(Database &database):
	_database(database)
{}

AccountCreationResult AccountRepository::createPendingAccountAndProfile(
	const std::string &email,
	const std::string &passwordHash,
	const std::string &username,
	const std::string &firstName,
	const std::string &lastName,
	const std::string &verificationTokenHash,
	std::chrono::seconds verificationLifetime
)
{
	if (verificationLifetime <= std::chrono::seconds::zero())
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};

	QueryResult result = this->_database.executeParams(
		"WITH account AS ("
		"  INSERT INTO auth_accounts "
		"    (email, password_hash, email_verification_last_sent_at) "
		"  VALUES ($1, $2, NOW()) "
		"  RETURNING id"
		"), profile AS ("
		"  INSERT INTO matcha_profiles "
		"    (account_id, username, first_name, last_name) "
		"  SELECT id, $3, $4, $5 FROM account "
		"  RETURNING account_id"
		"), verification_token AS ("
		"  INSERT INTO email_verification_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT account_id, $6, "
		"    NOW() + ($7 || ' seconds')::interval "
		"  FROM profile "
		"  RETURNING account_id"
		") "
		"SELECT account_id FROM verification_token",
		{
			email,
			passwordHash,
			username,
			firstName,
			lastName,
			verificationTokenHash,
			std::to_string(verificationLifetime.count())
		}
	);

	if (result.isError())
	{
		if (result.sqlState() == "23505")
			return {
				AccountCreationResult::Status::IdentifierAlreadyExists,
				0
			};
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};
	}

	if (result.rowCount() != 1)
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};

	const auto accountId = result.getInt64(0, 0);
	if (!accountId.has_value())
		return {
			AccountCreationResult::Status::DatabaseError,
			0
		};

	return {
		AccountCreationResult::Status::Success,
		*accountId
	};
}

EmailVerificationResult AccountRepository::verifyEmailToken(
	const std::string &tokenHash
)
{
	QueryResult result = this->_database.executeParams(
		"WITH matched AS ("
		"  SELECT id, account_id FROM email_verification_tokens "
		"  WHERE token_hash = $1 AND consumed_at IS NULL "
		"    AND expires_at > NOW() "
		"  FOR UPDATE"
		"), consumed AS ("
		"  UPDATE email_verification_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM matched m "
		"  WHERE t.id = m.id "
		"  RETURNING t.account_id"
		"), verified AS ("
		"  UPDATE auth_accounts a "
		"  SET email_verified_at = NOW(), updated_at = NOW() "
		"  FROM consumed c "
		"  WHERE a.id = c.account_id AND a.email_verified_at IS NULL "
		"  RETURNING a.id"
		") "
		"SELECT id FROM verified",
		{tokenHash}
	);

	if (result.isError())
		return {EmailVerificationResult::Status::DatabaseError};
	if (result.rowCount() == 1)
		return {EmailVerificationResult::Status::Verified};
	return {EmailVerificationResult::Status::InvalidOrExpired};
}

ResendVerificationResult AccountRepository::issueEmailVerificationToken(
	const std::string &email,
	const std::string &tokenHash,
	std::chrono::seconds tokenLifetime
)
{
	if (tokenLifetime <= std::chrono::seconds::zero())
		return {ResendVerificationResult::Status::DatabaseError, {}};

	QueryResult result = this->_database.executeParams(
		"WITH eligible AS ("
		"  UPDATE auth_accounts "
		"  SET email_verification_last_sent_at = NOW(), updated_at = NOW() "
		"  WHERE LOWER(email) = LOWER($1) "
		"    AND email_verified_at IS NULL "
		"    AND (email_verification_last_sent_at IS NULL "
		"      OR email_verification_last_sent_at <= NOW() - INTERVAL '60 seconds') "
		"  RETURNING id, email"
		"), consumed AS ("
		"  UPDATE email_verification_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM eligible e "
		"  WHERE t.account_id = e.id AND t.consumed_at IS NULL "
		"  RETURNING t.id"
		"), inserted AS ("
		"  INSERT INTO email_verification_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT e.id, $2, NOW() + ($3 || ' seconds')::interval "
		"  FROM eligible e "
		"  WHERE (SELECT COUNT(*) FROM consumed) >= 0 "
		"  RETURNING account_id"
		") "
		"SELECT e.email FROM eligible e "
		"JOIN inserted i ON i.account_id = e.id",
		{
			email,
			tokenHash,
			std::to_string(tokenLifetime.count())
		}
	);

	if (result.isError())
		return {ResendVerificationResult::Status::DatabaseError, {}};
	if (result.rowCount() != 1)
		return {ResendVerificationResult::Status::Suppressed, {}};

	const auto issuedEmail = result.getString(0, 0);
	if (!issuedEmail.has_value())
		return {ResendVerificationResult::Status::DatabaseError, {}};
	return {ResendVerificationResult::Status::Issued, *issuedEmail};
}

SessionResult AccountRepository::createSession(
	std::int64_t accountId,
	const std::string &refreshTokenHash,
	std::chrono::seconds idleLifetime,
	std::chrono::seconds absoluteLifetime
)
{
	if (idleLifetime <= std::chrono::seconds::zero() ||
		absoluteLifetime < idleLifetime)
		return {SessionResult::Status::DatabaseError, 0, {}};

	QueryResult result = this->_database.executeParams(
		"WITH account AS ("
		"  SELECT id, email FROM auth_accounts "
		"  WHERE id = $1 AND email_verified_at IS NOT NULL"
		"), session AS ("
		"  INSERT INTO user_sessions "
		"    (account_id, idle_expires_at, absolute_expires_at) "
		"  SELECT id, NOW() + ($3 || ' seconds')::interval, "
		"    NOW() + ($4 || ' seconds')::interval "
		"  FROM account "
		"  RETURNING id, account_id, idle_expires_at"
		"), refresh AS ("
		"  INSERT INTO refresh_tokens (session_id, token_hash, expires_at) "
		"  SELECT id, $2, idle_expires_at FROM session "
		"  RETURNING session_id"
		") "
		"SELECT a.id, a.email FROM account a "
		"JOIN session s ON s.account_id = a.id "
		"JOIN refresh r ON r.session_id = s.id",
		{
			std::to_string(accountId),
			refreshTokenHash,
			std::to_string(idleLifetime.count()),
			std::to_string(absoluteLifetime.count())
		}
	);
	if (result.isError())
		return {SessionResult::Status::DatabaseError, 0, {}};
	if (result.rowCount() != 1)
		return {SessionResult::Status::InvalidToken, 0, {}};

	const auto id = result.getInt64(0, 0);
	const auto email = result.getString(0, 1);
	if (!id.has_value() || !email.has_value())
		return {SessionResult::Status::DatabaseError, 0, {}};
	return {SessionResult::Status::Success, *id, *email};
}

SessionResult AccountRepository::rotateRefreshToken(
	const std::string &currentTokenHash,
	const std::string &replacementTokenHash,
	std::chrono::seconds idleLifetime
)
{
	if (idleLifetime <= std::chrono::seconds::zero())
		return {SessionResult::Status::DatabaseError, 0, {}};

	QueryResult result = this->_database.executeParams(
		"WITH candidate AS ("
		"  SELECT t.id AS token_id, t.session_id, t.expires_at AS token_expires_at, "
		"    t.consumed_at, s.account_id, s.absolute_expires_at, "
		"    s.idle_expires_at, s.revoked_at "
		"  FROM refresh_tokens t "
		"  JOIN user_sessions s ON s.id = t.session_id "
		"  WHERE t.token_hash = $1 "
		"  FOR UPDATE OF t, s"
		"), valid AS ("
		"  SELECT * FROM candidate "
		"  WHERE consumed_at IS NULL AND token_expires_at > NOW() "
		"    AND idle_expires_at > NOW() AND absolute_expires_at > NOW() "
		"    AND revoked_at IS NULL"
		"), consumed AS ("
		"  UPDATE refresh_tokens t SET consumed_at = NOW() "
		"  FROM valid v WHERE t.id = v.token_id "
		"  RETURNING t.session_id"
		"), extended AS ("
		"  UPDATE user_sessions s "
		"  SET last_used_at = NOW(), "
		"    idle_expires_at = LEAST("
		"      NOW() + ($3 || ' seconds')::interval, "
		"      s.absolute_expires_at"
		"    ) "
		"  FROM consumed c WHERE s.id = c.session_id "
		"  RETURNING s.id, s.account_id, s.idle_expires_at"
		"), inserted AS ("
		"  INSERT INTO refresh_tokens (session_id, token_hash, expires_at) "
		"  SELECT id, $2, idle_expires_at FROM extended "
		"  RETURNING session_id"
		"), replayed AS ("
		"  UPDATE user_sessions s SET revoked_at = NOW() "
		"  FROM candidate c "
		"  WHERE s.id = c.session_id AND c.consumed_at IS NOT NULL "
		"    AND s.revoked_at IS NULL "
		"  RETURNING s.id"
		") "
		"SELECT a.id, a.email FROM inserted i "
		"JOIN user_sessions s ON s.id = i.session_id "
		"JOIN auth_accounts a ON a.id = s.account_id",
		{
			currentTokenHash,
			replacementTokenHash,
			std::to_string(idleLifetime.count())
		}
	);
	if (result.isError())
		return {SessionResult::Status::DatabaseError, 0, {}};
	if (result.rowCount() != 1)
		return {SessionResult::Status::InvalidToken, 0, {}};

	const auto id = result.getInt64(0, 0);
	const auto email = result.getString(0, 1);
	if (!id.has_value() || !email.has_value())
		return {SessionResult::Status::DatabaseError, 0, {}};
	return {SessionResult::Status::Success, *id, *email};
}

bool AccountRepository::revokeSessionByRefreshToken(
	const std::string &refreshTokenHash
)
{
	QueryResult result = this->_database.executeParams(
		"UPDATE user_sessions s SET revoked_at = NOW() "
		"FROM refresh_tokens t "
		"WHERE t.token_hash = $1 AND t.session_id = s.id "
		"AND s.revoked_at IS NULL",
		{refreshTokenHash}
	);
	return !result.isError();
}

bool AccountRepository::revokeAllSessions(std::int64_t accountId)
{
	QueryResult result = this->_database.executeParams(
		"UPDATE user_sessions SET revoked_at = NOW() "
		"WHERE account_id = $1 AND revoked_at IS NULL",
		{std::to_string(accountId)}
	);
	return !result.isError();
}

PasswordResetRequestResult AccountRepository::issuePasswordResetToken(
	const std::string &email,
	const std::string &tokenHash,
	std::chrono::seconds tokenLifetime
)
{
	if (tokenLifetime <= std::chrono::seconds::zero())
		return {PasswordResetRequestResult::Status::DatabaseError, {}};

	QueryResult result = this->_database.executeParams(
		"WITH eligible AS ("
		"  UPDATE auth_accounts "
		"  SET password_reset_last_sent_at = NOW(), updated_at = NOW() "
		"  WHERE LOWER(email) = LOWER($1) "
		"    AND email_verified_at IS NOT NULL "
		"    AND (password_reset_last_sent_at IS NULL "
		"      OR password_reset_last_sent_at <= NOW() - INTERVAL '60 seconds') "
		"  RETURNING id, email"
		"), consumed AS ("
		"  UPDATE password_reset_tokens t SET consumed_at = NOW() "
		"  FROM eligible e "
		"  WHERE t.account_id = e.id AND t.consumed_at IS NULL "
		"  RETURNING t.id"
		"), inserted AS ("
		"  INSERT INTO password_reset_tokens "
		"    (account_id, token_hash, expires_at) "
		"  SELECT e.id, $2, NOW() + ($3 || ' seconds')::interval "
		"  FROM eligible e "
		"  WHERE (SELECT COUNT(*) FROM consumed) >= 0 "
		"  RETURNING account_id"
		") "
		"SELECT e.email FROM eligible e "
		"JOIN inserted i ON i.account_id = e.id",
		{
			email,
			tokenHash,
			std::to_string(tokenLifetime.count())
		}
	);
	if (result.isError())
		return {PasswordResetRequestResult::Status::DatabaseError, {}};
	if (result.rowCount() != 1)
		return {PasswordResetRequestResult::Status::Suppressed, {}};

	const auto issuedEmail = result.getString(0, 0);
	if (!issuedEmail.has_value())
		return {PasswordResetRequestResult::Status::DatabaseError, {}};
	return {PasswordResetRequestResult::Status::Issued, *issuedEmail};
}

PasswordResetResult AccountRepository::resetPassword(
	const std::string &tokenHash,
	const std::string &passwordHash
)
{
	QueryResult result = this->_database.executeParams(
		"WITH candidate AS ("
		"  SELECT t.id AS token_id, t.account_id "
		"  FROM password_reset_tokens t "
		"  JOIN auth_accounts a ON a.id = t.account_id "
		"  WHERE t.token_hash = $1 AND t.consumed_at IS NULL "
		"    AND t.expires_at > NOW() AND a.email_verified_at IS NOT NULL "
		"  FOR UPDATE OF t, a"
		"), updated_account AS ("
		"  UPDATE auth_accounts a "
		"  SET password_hash = $2, updated_at = NOW() "
		"  FROM candidate c WHERE a.id = c.account_id "
		"  RETURNING a.id"
		"), consumed_tokens AS ("
		"  UPDATE password_reset_tokens t "
		"  SET consumed_at = NOW() "
		"  FROM updated_account a "
		"  WHERE t.account_id = a.id AND t.consumed_at IS NULL "
		"  RETURNING t.account_id"
		"), revoked_sessions AS ("
		"  UPDATE user_sessions s SET revoked_at = NOW() "
		"  FROM updated_account a "
		"  WHERE s.account_id = a.id AND s.revoked_at IS NULL "
		"  RETURNING s.id"
		") "
		"SELECT id FROM updated_account",
		{tokenHash, passwordHash}
	);
	if (result.isError())
		return {PasswordResetResult::Status::DatabaseError};
	if (result.rowCount() == 1)
		return {PasswordResetResult::Status::Reset};
	return {PasswordResetResult::Status::InvalidOrExpired};
}

auth::UserLookupResult AccountRepository::findByEmail(
	const std::string &email
)
{
	QueryResult result = this->_database.executeParams(
		"SELECT id, email, password_hash FROM auth_accounts "
		"WHERE LOWER(email) = LOWER($1) AND email_verified_at IS NOT NULL",
		{email}
	);
	if (result.isError())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	if (result.rowCount() == 0)
		return {
			auth::UserLookupResult::Status::NotFound,
			std::nullopt
		};

	const auto id = result.getInt64(0, 0);
	const auto userEmail = result.getString(0, 1);
	const auto passwordHash = result.getString(0, 2);
	if (!id.has_value() || !userEmail.has_value() ||
		!passwordHash.has_value())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	return {
		auth::UserLookupResult::Status::Found,
		auth::User{*id, *userEmail, *passwordHash}
	};
}

auth::UserLookupResult AccountRepository::findById(
	const std::int64_t id
)
{
	QueryResult result = this->_database.executeParams(
		"SELECT id, email, password_hash FROM auth_accounts "
		"WHERE id = $1 AND email_verified_at IS NOT NULL",
		{std::to_string(id)}
	);
	if (result.isError())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	if (result.rowCount() == 0)
		return {
			auth::UserLookupResult::Status::NotFound,
			std::nullopt
		};

	const auto userId = result.getInt64(0, 0);
	const auto userEmail = result.getString(0, 1);
	const auto passwordHash = result.getString(0, 2);
	if (!userId.has_value() || !userEmail.has_value() ||
		!passwordHash.has_value())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	return {
		auth::UserLookupResult::Status::Found,
		auth::User{*userId, *userEmail, *passwordHash}
	};
}
