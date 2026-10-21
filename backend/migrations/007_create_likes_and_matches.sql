CREATE TABLE profile_likes (
    id BIGSERIAL PRIMARY KEY,
    liker_account_id BIGINT NOT NULL REFERENCES auth_accounts(id) ON DELETE CASCADE,
    liked_account_id BIGINT NOT NULL REFERENCES auth_accounts(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT profile_likes_no_self_like
        CHECK (liker_account_id <> liked_account_id),
    CONSTRAINT profile_likes_unique_pair
        UNIQUE (liker_account_id, liked_account_id)
);

CREATE INDEX profile_likes_liked_account_idx
    ON profile_likes (liked_account_id, created_at DESC);

CREATE TABLE profile_matches (
    id BIGSERIAL PRIMARY KEY,
    first_account_id BIGINT NOT NULL REFERENCES auth_accounts(id) ON DELETE CASCADE,
    second_account_id BIGINT NOT NULL REFERENCES auth_accounts(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT profile_matches_canonical_pair
        CHECK (first_account_id < second_account_id),
    CONSTRAINT profile_matches_unique_pair
        UNIQUE (first_account_id, second_account_id)
);

CREATE INDEX profile_matches_second_account_idx
    ON profile_matches (second_account_id, created_at DESC);
