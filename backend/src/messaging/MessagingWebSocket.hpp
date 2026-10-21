#pragma once

#include "MessagingRepository.hpp"
#include "../http/AuthRoutes.hpp"

#include <auth/auth/Authenticator.hpp>
#include <crow.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class MessagingWebSocketHub
{
	public:
		void add(std::int64_t accountId, crow::websocket::connection *connection);
		void remove(
			std::int64_t accountId, crow::websocket::connection *connection);
		void sendToAccount(std::int64_t accountId, const std::string &payload);

	private:
		std::mutex _mutex;
		std::unordered_map<
			std::int64_t,
			std::unordered_set<crow::websocket::connection *>> _connections;
};

void registerMessagingWebSocket(
	AuthApp &app,
	auth::Authenticator &authenticator,
	MessagingRepository &messagingRepository,
	MessagingWebSocketHub &hub
);
