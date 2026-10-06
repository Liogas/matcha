#pragma once

#include <auth/credentials/AuthService.hpp>
#include <auth/auth/Authenticator.hpp>
#include "AuthRequest.hpp"

#include <crow.h>

void	registerAuthRoutes(
	crow::SimpleApp &app,
	auth::AuthService &authService,
	auth::Authenticator &authenticator
);