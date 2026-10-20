CREATE TABLE auth_accounts (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	email TEXT NOT NULL,
	password_hash TEXT NOT NULL,
	email_verified_at TIMESTAMPTZ,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	CONSTRAINT auth_accounts_email_length
		CHECK (length(email) BETWEEN 3 AND 254)
);

CREATE UNIQUE INDEX auth_accounts_email_lower_unique
	ON auth_accounts (LOWER(email));

CREATE TABLE matcha_profiles (
	account_id BIGINT PRIMARY KEY
		REFERENCES auth_accounts(id) ON DELETE CASCADE,
	username VARCHAR(30) NOT NULL,
	first_name VARCHAR(80) NOT NULL,
	last_name VARCHAR(80) NOT NULL,
	date_of_birth DATE,
	gender TEXT,
	sexual_orientation TEXT NOT NULL DEFAULT 'bisexual',
	biography TEXT,
	city TEXT,
	location_source TEXT NOT NULL DEFAULT 'manual'
		CHECK (location_source IN ('manual', 'gps_consent')),
	location_consent_at TIMESTAMPTZ,
	completed_at TIMESTAMPTZ,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	CONSTRAINT matcha_profiles_username_format
		CHECK (username ~ '^[A-Za-z0-9_]{3,30}$'),
	CONSTRAINT matcha_profiles_first_name_nonempty
		CHECK (length(BTRIM(first_name)) > 0),
	CONSTRAINT matcha_profiles_last_name_nonempty
		CHECK (length(BTRIM(last_name)) > 0),
	CONSTRAINT matcha_profiles_location_consent
		CHECK (
			(location_source = 'manual' AND location_consent_at IS NULL)
			OR
			(location_source = 'gps_consent' AND location_consent_at IS NOT NULL)
		)
);

CREATE UNIQUE INDEX matcha_profiles_username_lower_unique
	ON matcha_profiles (LOWER(username));

CREATE TABLE email_verification_tokens (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	account_id BIGINT NOT NULL
		REFERENCES auth_accounts(id) ON DELETE CASCADE,
	token_hash CHAR(64) NOT NULL UNIQUE
		CHECK (token_hash ~ '^[a-f0-9]{64}$'),
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	expires_at TIMESTAMPTZ NOT NULL,
	consumed_at TIMESTAMPTZ,
	CHECK (expires_at > created_at),
	CHECK (consumed_at IS NULL OR consumed_at >= created_at)
);

CREATE INDEX email_verification_tokens_account_idx
	ON email_verification_tokens (account_id);

CREATE TABLE password_reset_tokens (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	account_id BIGINT NOT NULL
		REFERENCES auth_accounts(id) ON DELETE CASCADE,
	token_hash CHAR(64) NOT NULL UNIQUE
		CHECK (token_hash ~ '^[a-f0-9]{64}$'),
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	expires_at TIMESTAMPTZ NOT NULL,
	consumed_at TIMESTAMPTZ,
	CHECK (expires_at > created_at),
	CHECK (consumed_at IS NULL OR consumed_at >= created_at)
);

CREATE INDEX password_reset_tokens_account_idx
	ON password_reset_tokens (account_id);

CREATE TABLE user_sessions (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	account_id BIGINT NOT NULL
		REFERENCES auth_accounts(id) ON DELETE CASCADE,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	last_used_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	idle_expires_at TIMESTAMPTZ NOT NULL,
	absolute_expires_at TIMESTAMPTZ NOT NULL,
	revoked_at TIMESTAMPTZ,
	CHECK (idle_expires_at <= absolute_expires_at),
	CHECK (absolute_expires_at > created_at),
	CHECK (revoked_at IS NULL OR revoked_at >= created_at)
);

CREATE INDEX user_sessions_active_account_idx
	ON user_sessions (account_id, absolute_expires_at)
	WHERE revoked_at IS NULL;

CREATE TABLE refresh_tokens (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	session_id BIGINT NOT NULL
		REFERENCES user_sessions(id) ON DELETE CASCADE,
	token_hash CHAR(64) NOT NULL UNIQUE
		CHECK (token_hash ~ '^[a-f0-9]{64}$'),
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	expires_at TIMESTAMPTZ NOT NULL,
	consumed_at TIMESTAMPTZ,
	replaced_by_token_id BIGINT
		REFERENCES refresh_tokens(id) ON DELETE SET NULL,
	CHECK (expires_at > created_at),
	CHECK (consumed_at IS NULL OR consumed_at >= created_at),
	CHECK (replaced_by_token_id IS NULL OR consumed_at IS NOT NULL)
);

CREATE INDEX refresh_tokens_session_idx
	ON refresh_tokens (session_id);

CREATE TABLE matcha_interests (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	name VARCHAR(40) NOT NULL,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	CONSTRAINT matcha_interests_name_format
		CHECK (name ~ '^[[:alnum:]_#-]{1,40}$')
);

CREATE UNIQUE INDEX matcha_interests_name_lower_unique
	ON matcha_interests (LOWER(name));

CREATE TABLE matcha_profile_interests (
	profile_account_id BIGINT NOT NULL
		REFERENCES matcha_profiles(account_id) ON DELETE CASCADE,
	interest_id BIGINT NOT NULL
		REFERENCES matcha_interests(id) ON DELETE RESTRICT,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	PRIMARY KEY (profile_account_id, interest_id)
);

CREATE TABLE matcha_profile_photos (
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	profile_account_id BIGINT NOT NULL
		REFERENCES matcha_profiles(account_id) ON DELETE CASCADE,
	storage_key TEXT NOT NULL UNIQUE,
	position SMALLINT NOT NULL CHECK (position BETWEEN 1 AND 5),
	is_primary BOOLEAN NOT NULL DEFAULT FALSE,
	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	UNIQUE (profile_account_id, position)
);

CREATE UNIQUE INDEX matcha_profile_photos_one_primary
	ON matcha_profile_photos (profile_account_id)
	WHERE is_primary;
