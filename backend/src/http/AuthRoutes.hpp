#pragma once

#include <auth/credentials/AuthService.hpp>
#include "AuthRequest.hpp"

#include <crow.h>

void	registerAuthRoutes(
	crow::SimpleApp &app,
	auth::AuthService &authService
);