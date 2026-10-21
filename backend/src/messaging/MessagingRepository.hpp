#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Database;

struct MessagingConversation
{
	std::int64_t id;
	std::int64_t otherParticipantId;
	std::int64_t unreadCount;
};

struct MessagingMessage
{
	std::int64_t id;
	std::int64_t conversationId;
	std::int64_t senderAccountId;
	std::string body;
	std::string createdAt;
	bool read;
};

class MessagingRepository
{
	public:
		explicit MessagingRepository(Database &database);

		std::optional<std::int64_t> ensureDirectConversation(
			std::int64_t firstAccountId, std::int64_t secondAccountId);
		bool removeDirectConversation(
			std::int64_t firstAccountId, std::int64_t secondAccountId);
		std::vector<MessagingConversation> listConversations(
			std::int64_t accountId);
		std::vector<MessagingMessage> listMessages(
			std::int64_t accountId, std::int64_t conversationId,
			std::int64_t beforeId, std::int32_t limit);
		std::optional<MessagingMessage> sendMessage(
			std::int64_t accountId, std::int64_t conversationId,
			const std::string &body);
		std::optional<std::int64_t> otherParticipant(
			std::int64_t accountId, std::int64_t conversationId);
		bool markRead(std::int64_t accountId, std::int64_t conversationId);

	private:
		Database &_database;
};
