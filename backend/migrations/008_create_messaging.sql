CREATE TABLE messaging_conversations (
    id BIGSERIAL PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE messaging_conversation_participants (
    conversation_id BIGINT NOT NULL
        REFERENCES messaging_conversations(id) ON DELETE CASCADE,
    account_id BIGINT NOT NULL
        REFERENCES auth_accounts(id) ON DELETE CASCADE,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (conversation_id, account_id)
);

CREATE INDEX messaging_participants_account_idx
    ON messaging_conversation_participants (account_id, conversation_id);

CREATE TABLE messaging_messages (
    id BIGSERIAL PRIMARY KEY,
    conversation_id BIGINT NOT NULL
        REFERENCES messaging_conversations(id) ON DELETE CASCADE,
    sender_account_id BIGINT NOT NULL
        REFERENCES auth_accounts(id) ON DELETE CASCADE,
    body TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    read_at TIMESTAMPTZ,
    CONSTRAINT messaging_messages_body_length
        CHECK (char_length(BTRIM(body)) BETWEEN 1 AND 2000)
);

CREATE INDEX messaging_messages_conversation_idx
    ON messaging_messages (conversation_id, id DESC);

CREATE INDEX messaging_messages_sender_created_idx
    ON messaging_messages (sender_account_id, created_at DESC);
