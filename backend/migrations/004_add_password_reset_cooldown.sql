ALTER TABLE auth_accounts
	ADD COLUMN password_reset_last_sent_at TIMESTAMPTZ;

UPDATE auth_accounts a
SET password_reset_last_sent_at = (
	SELECT MAX(t.created_at)
	FROM password_reset_tokens t
	WHERE t.account_id = a.id
);
