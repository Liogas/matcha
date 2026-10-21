#pragma once

#include "../http/AuthRoutes.hpp"
#include "MessagingRepository.hpp"

void registerMessagingRoutes(
	AuthApp &app,
	auth::Authenticator &authenticator,
	MessagingRepository &messagingRepository
);
