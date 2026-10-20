ALTER TABLE auth_accounts
	ADD COLUMN email_verification_last_sent_at TIMESTAMPTZ;

UPDATE auth_accounts a
SET email_verification_last_sent_at = (
	SELECT MAX(t.created_at)
	FROM email_verification_tokens t
	WHERE t.account_id = a.id
);
