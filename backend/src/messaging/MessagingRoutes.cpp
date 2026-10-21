#include "MessagingRoutes.hpp"

#include "../http/AuthRouteSupport.hpp"

#include <charconv>
#include <iostream>

namespace
{
	crow::response unauthorized()
	{
		return auth_routes::jsonError(401, "Missing or invalid bearer token");
	}

	bool parsePositive(const char *value, std::int64_t &output)
	{
		if (!value)
			return false;
		const auto end = value + std::char_traits<char>::length(value);
		const auto parsed = std::from_chars(value, end, output);
		return parsed.ec == std::errc{} && parsed.ptr == end && output > 0;
	}
}

void registerMessagingRoutes(
	AuthApp &app, auth::Authenticator &authenticator,
	MessagingRepository &messagingRepository)
{
	CROW_ROUTE(app, "/api/conversations")
		.methods(crow::HTTPMethod::GET)(
		[&](const crow::request &request) {
			const auto account = auth_routes::authenticatedAccountId(
				request, authenticator);
			if (!account)
				return unauthorized();
			try
			{
				crow::json::wvalue::list items;
				for (const auto &conversation :
					messagingRepository.listConversations(*account))
				{
					crow::json::wvalue item;
					item["id"] = conversation.id;
					item["other_account_id"] = conversation.otherParticipantId;
					item["unread_count"] = conversation.unreadCount;
					items.emplace_back(std::move(item));
				}
				crow::json::wvalue body;
				body["conversations"] = std::move(items);
				return crow::response(200, body);
			}
			catch (const std::exception &error)
			{
				std::cerr << "[MESSAGING ERROR] " << error.what() << std::endl;
				return auth_routes::jsonError(500, "Unable to list conversations");
			}
		});

	CROW_ROUTE(app, "/api/conversations/<int>/messages")
		.methods(crow::HTTPMethod::GET)(
		[&](const crow::request &request, int conversationId) {
			const auto account = auth_routes::authenticatedAccountId(
				request, authenticator);
			if (!account)
				return unauthorized();
			std::int64_t before = 0;
			std::int64_t limit = 50;
			if (request.url_params.get("before") &&
				!parsePositive(request.url_params.get("before"), before))
				return auth_routes::jsonError(400, "Invalid before cursor");
			if (request.url_params.get("limit") &&
				!parsePositive(request.url_params.get("limit"), limit))
				return auth_routes::jsonError(400, "Invalid message limit");
			if (limit > 100)
				return auth_routes::jsonError(400, "Message limit must be at most 100");
			try
			{
				crow::json::wvalue::list items;
				for (const auto &message : messagingRepository.listMessages(
					*account, conversationId, before,
					static_cast<std::int32_t>(limit)))
				{
					crow::json::wvalue item;
					item["id"] = message.id;
					item["conversation_id"] = message.conversationId;
					item["sender_account_id"] = message.senderAccountId;
					item["body"] = message.body;
					item["created_at"] = message.createdAt;
					item["read"] = message.read;
					items.emplace_back(std::move(item));
				}
				crow::json::wvalue body;
				body["messages"] = std::move(items);
				return crow::response(200, body);
			}
			catch (const std::exception &error)
			{
				std::cerr << "[MESSAGING ERROR] " << error.what() << std::endl;
				return auth_routes::jsonError(500, "Unable to list messages");
			}
		});

	CROW_ROUTE(app, "/api/conversations/<int>/messages")
		.methods(crow::HTTPMethod::POST)(
		[&](const crow::request &request, int conversationId) {
			const auto account = auth_routes::authenticatedAccountId(
				request, authenticator);
			if (!account)
				return unauthorized();
			const auto body = crow::json::load(request.body);
			if (!body || !body.has("body") ||
				body["body"].t() != crow::json::type::String)
				return auth_routes::jsonError(400, "Message body is required");
			const std::string text = body["body"].s();
			if (text.size() > 2000 || auth_routes::trim(text).empty())
				return auth_routes::jsonError(400,
					"Message must contain 1-2000 characters");
			try
			{
				const auto message = messagingRepository.sendMessage(
					*account, conversationId, text);
				if (!message)
					return auth_routes::jsonError(403,
						"Conversation access denied or rate limit reached");
				crow::json::wvalue response;
				response["id"] = message->id;
				response["conversation_id"] = message->conversationId;
				response["sender_account_id"] = message->senderAccountId;
				response["body"] = message->body;
				response["created_at"] = message->createdAt;
				response["read"] = false;
				return crow::response(201, response);
			}
			catch (const std::exception &error)
			{
				std::cerr << "[MESSAGING ERROR] " << error.what() << std::endl;
				return auth_routes::jsonError(500, "Unable to send message");
			}
		});

	CROW_ROUTE(app, "/api/conversations/<int>/read")
		.methods(crow::HTTPMethod::POST)(
		[&](const crow::request &request, int conversationId) {
			const auto account = auth_routes::authenticatedAccountId(
				request, authenticator);
			if (!account)
				return unauthorized();
			if (!messagingRepository.markRead(*account, conversationId))
				return auth_routes::jsonError(403, "Conversation access denied");
			return auth_routes::jsonMessage(200, "Messages marked as read");
		});
}
