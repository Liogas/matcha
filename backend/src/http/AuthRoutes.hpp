#pragma once

#include <auth/credentials/AuthService.hpp>
#include <auth/auth/Authenticator.hpp>
#include <auth/token/RsaTokenSigner.hpp>
#include <auth/token/TokenGenerator.hpp>
#include "AuthRequest.hpp"
#include "../security/RedisLoginAttemptLimiter.hpp"

#include <crow.h>

void	registerAuthRoutes(
	crow::SimpleApp &app,
	auth::AuthService &authService,
	auth::Authenticator &authenticator,
	auth::TokenGenerator &tokenGenerator,
	RedisLoginAttemptLimiter &loginAttemptLimiter
);

void registerJwksRoute(
	crow::SimpleApp &app,
	const auth::RsaTokenSigner &tokenSigner
);