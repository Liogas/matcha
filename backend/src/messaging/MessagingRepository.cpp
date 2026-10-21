#include "MessagingRepository.hpp"

#include "../database/Database.hpp"

#include <stdexcept>

namespace
{
	std::optional<MessagingMessage> readMessage(
		const QueryResult &result, int row, std::int64_t conversationId)
	{
		const auto id = result.getInt64(row, 0);
		const auto sender = result.getInt64(row, 1);
		const auto body = result.getString(row, 2);
		const auto created = result.getString(row, 3);
		const auto read = result.getBool(row, 4);
		if (!id || !sender || !body || !created || !read)
			return std::nullopt;
		return MessagingMessage{
			*id, conversationId, *sender, *body, *created, *read};
	}
}

MessagingRepository::MessagingRepository(Database &database):
	_database(database)
{}

std::optional<std::int64_t> MessagingRepository::ensureDirectConversation(
	std::int64_t firstAccountId, std::int64_t secondAccountId)
{
	if (firstAccountId <= 0 || secondAccountId <= 0 ||
		firstAccountId == secondAccountId)
		return std::nullopt;
	const auto result = _database.executeParams(R"(
		WITH locked AS (
			SELECT pg_advisory_xact_lock(
				hashtextextended(
					LEAST($1::BIGINT, $2::BIGINT)::TEXT || ':' ||
					GREATEST($1::BIGINT, $2::BIGINT)::TEXT,
					0
				)
			)
		), existing AS (
			SELECT c.id
			FROM messaging_conversations c
			JOIN messaging_conversation_participants p
				ON p.conversation_id = c.id
			CROSS JOIN locked
			WHERE p.account_id IN ($1, $2)
			GROUP BY c.id
			HAVING COUNT(DISTINCT p.account_id) = 2
			AND COUNT(*) = 2
			LIMIT 1
		), created AS (
			INSERT INTO messaging_conversations
			SELECT FROM (SELECT 1) AS seed
			WHERE NOT EXISTS (SELECT 1 FROM existing)
			RETURNING id
		), participants AS (
			INSERT INTO messaging_conversation_participants
				(conversation_id, account_id)
			SELECT id, $1 FROM created
			UNION ALL
			SELECT id, $2 FROM created
			RETURNING conversation_id
		), selected AS (
			SELECT id FROM existing
			UNION ALL
			SELECT conversation_id FROM participants
		)
		SELECT id FROM selected
		LIMIT 1
	)", {std::to_string(firstAccountId), std::to_string(secondAccountId)});
	if (result.isError() || result.rowCount() != 1)
		return std::nullopt;
	return result.getInt64(0, 0);
}

bool MessagingRepository::removeDirectConversation(
	std::int64_t firstAccountId, std::int64_t secondAccountId)
{
	const auto result = _database.executeParams(R"(
		DELETE FROM messaging_conversations c
		WHERE c.id IN (
			SELECT p.conversation_id
			FROM messaging_conversation_participants p
			WHERE p.account_id IN ($1, $2)
			GROUP BY p.conversation_id
			HAVING COUNT(DISTINCT p.account_id) = 2 AND COUNT(*) = 2
		)
	)", {std::to_string(firstAccountId), std::to_string(secondAccountId)});
	return !result.isError();
}

std::vector<MessagingConversation> MessagingRepository::listConversations(
	std::int64_t accountId)
{
	const auto result = _database.executeParams(R"(
		SELECT c.id,
			(SELECT p2.account_id
			 FROM messaging_conversation_participants p2
			 WHERE p2.conversation_id = c.id AND p2.account_id <> $1
			 LIMIT 1),
			(SELECT COUNT(*) FROM messaging_messages m
			 WHERE m.conversation_id = c.id
			 AND m.sender_account_id <> $1 AND m.read_at IS NULL)
		FROM messaging_conversations c
		JOIN messaging_conversation_participants p
			ON p.conversation_id = c.id AND p.account_id = $1
		ORDER BY c.id DESC
	)", {std::to_string(accountId)});
	if (result.isError())
		throw std::runtime_error("Unable to list conversations");
	std::vector<MessagingConversation> conversations;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto id = result.getInt64(row, 0);
		const auto other = result.getInt64(row, 1);
		const auto unread = result.getInt64(row, 2);
		if (id && other && unread)
			conversations.push_back({*id, *other, *unread});
	}
	return conversations;
}

std::vector<MessagingMessage> MessagingRepository::listMessages(
	std::int64_t accountId, std::int64_t conversationId,
	std::int64_t beforeId, std::int32_t limit)
{
	const auto result = _database.executeParams(R"(
		SELECT m.id, m.sender_account_id, m.body, m.created_at::text,
			m.read_at IS NOT NULL
		FROM messaging_messages m
		JOIN messaging_conversation_participants p
			ON p.conversation_id = m.conversation_id
			AND p.account_id = $2
		WHERE m.conversation_id = $1
		AND ($3 = 0 OR m.id < $3)
		ORDER BY m.id DESC LIMIT $4
	)", {std::to_string(conversationId), std::to_string(accountId),
		std::to_string(beforeId), std::to_string(limit)});
	if (result.isError())
		throw std::runtime_error("Unable to list messages");
	std::vector<MessagingMessage> messages;
	for (int row = 0; row < result.rowCount(); ++row)
	{
		const auto message = readMessage(result, row, conversationId);
		if (message)
			messages.push_back(*message);
	}
	return messages;
}

std::optional<MessagingMessage> MessagingRepository::sendMessage(
	std::int64_t accountId, std::int64_t conversationId,
	const std::string &body)
{
	const auto result = _database.executeParams(R"(
		WITH participant AS (
			SELECT conversation_id FROM messaging_conversation_participants
			WHERE conversation_id = $1 AND account_id = $2
		), rate AS (
			SELECT COUNT(*) AS count FROM messaging_messages
			WHERE sender_account_id = $2
			AND created_at > NOW() - INTERVAL '1 minute'
		), inserted AS (
			INSERT INTO messaging_messages
				(conversation_id, sender_account_id, body)
			SELECT p.conversation_id, $2, BTRIM($3)
			FROM participant p, rate WHERE rate.count < 30
			RETURNING id, sender_account_id, body, created_at::text, false
		)
		SELECT * FROM inserted
	)", {std::to_string(conversationId), std::to_string(accountId), body});
	if (result.isError())
		throw std::runtime_error("Unable to send message");
	if (result.rowCount() != 1)
		return std::nullopt;
	return readMessage(result, 0, conversationId);
}

std::optional<std::int64_t> MessagingRepository::otherParticipant(
	std::int64_t accountId, std::int64_t conversationId)
{
	const auto result = _database.executeParams(R"(
		SELECT p.account_id
		FROM messaging_conversation_participants p
		WHERE p.conversation_id = $1 AND p.account_id <> $2
		AND EXISTS (
			SELECT 1 FROM messaging_conversation_participants own
			WHERE own.conversation_id = p.conversation_id
			AND own.account_id = $2
		)
		LIMIT 1
	)", {std::to_string(conversationId), std::to_string(accountId)});
	if (result.isError() || result.rowCount() != 1)
		return std::nullopt;
	return result.getInt64(0, 0);
}

bool MessagingRepository::markRead(
	std::int64_t accountId, std::int64_t conversationId)
{
	const auto result = _database.executeParams(R"(
		UPDATE messaging_messages m SET read_at = NOW()
		WHERE m.conversation_id = $1 AND m.sender_account_id <> $2
		AND m.read_at IS NULL
		AND EXISTS (
			SELECT 1 FROM messaging_conversation_participants p
			WHERE p.conversation_id = m.conversation_id
			AND p.account_id = $2
		)
	)", {std::to_string(conversationId), std::to_string(accountId)});
	return !result.isError();
}
