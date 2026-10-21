#include "MessagingWebSocket.hpp"

#include "../http/AuthRouteSupport.hpp"

#include <iostream>

namespace
{
	std::optional<std::int64_t> accountFromRequest(
		const crow::request &request,
		auth::Authenticator &authenticator)
	{
		if (const auto token = auth_routes::bearerToken(request))
		{
			const auto result = authenticator.authenticate(*token);
			if (result.status == auth::TokenValidationResult::Status::Valid &&
				result.claims)
			{
				std::int64_t accountId = 0;
				const auto &subject = result.claims->subject;
				const auto parsed = std::from_chars(
					subject.data(), subject.data() + subject.size(), accountId);
				if (parsed.ec == std::errc{} &&
					parsed.ptr == subject.data() + subject.size() &&
					accountId > 0)
					return accountId;
			}
		}

		// Browser WebSocket clients cannot set Authorization headers. The
		// access token is accepted as a query parameter only for this upgrade.
		const char *queryToken = request.url_params.get("access_token");
		if (!queryToken || *queryToken == '\0')
			return std::nullopt;
		const auto result = authenticator.authenticate(queryToken);
		if (result.status != auth::TokenValidationResult::Status::Valid ||
			!result.claims)
			return std::nullopt;
		std::int64_t accountId = 0;
		const auto &subject = result.claims->subject;
		const auto parsed = std::from_chars(
			subject.data(), subject.data() + subject.size(), accountId);
		if (parsed.ec != std::errc{} ||
			parsed.ptr != subject.data() + subject.size() || accountId <= 0)
			return std::nullopt;
		return accountId;
	}

	crow::json::wvalue messagePayload(
		const MessagingMessage &message, const char *type)
	{
		crow::json::wvalue payload;
		payload["type"] = type;
		payload["id"] = message.id;
		payload["conversation_id"] = message.conversationId;
		payload["sender_account_id"] = message.senderAccountId;
		payload["body"] = message.body;
		payload["created_at"] = message.createdAt;
		return payload;
	}
}

void MessagingWebSocketHub::add(
	std::int64_t accountId, crow::websocket::connection *connection)
{
	std::lock_guard lock(_mutex);
	_connections[accountId].insert(connection);
}

void MessagingWebSocketHub::remove(
	std::int64_t accountId, crow::websocket::connection *connection)
{
	std::lock_guard lock(_mutex);
	auto it = _connections.find(accountId);
	if (it == _connections.end())
		return;
	it->second.erase(connection);
	if (it->second.empty())
		_connections.erase(it);
}

void MessagingWebSocketHub::sendToAccount(
	std::int64_t accountId, const std::string &payload)
{
	std::lock_guard lock(_mutex);
	const auto it = _connections.find(accountId);
	if (it == _connections.end())
		return;
	for (auto *connection : it->second)
		connection->send_text(payload);
}

void registerMessagingWebSocket(
	AuthApp &app,
	auth::Authenticator &authenticator,
	MessagingRepository &messagingRepository,
	MessagingWebSocketHub &hub)
{
	CROW_WEBSOCKET_ROUTE(app, "/api/messaging/ws")
		.onaccept([&](const crow::request &request, void **userData) {
			const auto account = accountFromRequest(request, authenticator);
			if (!account)
				return false;
			*userData = new std::int64_t(*account);
			return true;
		})
		.onopen([&](crow::websocket::connection &connection) {
			auto *account = static_cast<std::int64_t *>(connection.userdata());
			if (account)
				hub.add(*account, &connection);
		})
		.onmessage([&](crow::websocket::connection &connection,
			const std::string &data, bool isBinary) {
			if (isBinary)
				return;
			auto *account = static_cast<std::int64_t *>(connection.userdata());
			if (!account)
				return;
			const auto request = crow::json::load(data);
			if (!request || !request.has("conversation_id") ||
				!request.has("body") ||
				request["conversation_id"].t() != crow::json::type::Number ||
				request["body"].t() != crow::json::type::String)
				return;
			const auto conversationId = request["conversation_id"].i();
			const auto body = request["body"].s();
			if (conversationId <= 0 || body.size() > 2000 ||
				auth_routes::trim(body).empty())
				return;
			try
			{
				const auto message = messagingRepository.sendMessage(
					*account, conversationId, body);
				if (!message)
					return;
				auto response = messagePayload(*message, "message");
				connection.send_text(response.dump());
				const auto recipient =
					messagingRepository.otherParticipant(
						*account, conversationId);
				if (recipient)
					hub.sendToAccount(*recipient, response.dump());
			}
			catch (const std::exception &error)
			{
				std::cerr << "[MESSAGING WEBSOCKET ERROR] "
					<< error.what() << std::endl;
			}
		})
		.onclose([&](crow::websocket::connection &connection,
			const std::string &, uint16_t) {
			auto *account = static_cast<std::int64_t *>(connection.userdata());
			if (account)
			{
				hub.remove(*account, &connection);
				delete account;
			}
		})
		.onerror([](crow::websocket::connection &, const std::string &error) {
			std::cerr << "[MESSAGING WEBSOCKET ERROR] " << error << std::endl;
		});
}
