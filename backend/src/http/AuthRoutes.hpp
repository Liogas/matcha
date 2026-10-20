#pragma once

#include <auth/credentials/AuthService.hpp>
#include <auth/auth/Authenticator.hpp>
#include <auth/token/RsaTokenSigner.hpp>
#include <auth/token/TokenGenerator.hpp>
#include "AuthRequest.hpp"
#include "../repositories/AccountRepository.hpp"
#include "../services/SmtpEmailSender.hpp"
#include "../security/PasswordHasher.hpp"
#include "../security/RedisLoginAttemptLimiter.hpp"

#include <crow.h>
#include <crow/middlewares/cors.h>

class AuthCorsHandler : public crow::CORSHandler
{
	public:
		std::string allowedOrigin;

		template<typename AllContext>
		void before_handle(
			crow::request &,
			crow::response &,
			context &,
			AllContext &
		)
		{}

		void after_handle(
			crow::request &request,
			crow::response &response,
			context &middlewareContext
		)
		{
			crow::CORSHandler::after_handle(
				request,
				response,
				middlewareContext
			);
			if (request.method == crow::HTTPMethod::Options &&
				request.get_header_value("Origin") == allowedOrigin)
				response.add_header(
					"Access-Control-Allow-Credentials",
					"true"
				);
		}
};

using AuthApp = crow::App<AuthCorsHandler>;

void	registerAuthRoutes(
	AuthApp &app,
	auth::AuthService &authService,
	auth::Authenticator &authenticator,
	auth::TokenGenerator &tokenGenerator,
	RedisLoginAttemptLimiter &loginAttemptLimiter,
	AccountRepository &accountRepository,
	auth::PasswordHasher &passwordHasher,
	const SmtpEmailSender &emailSender,
	const std::string &emailVerificationUrl,
	const std::string &passwordResetUrl,
	const std::string &allowedBrowserOrigin,
	bool secureRefreshCookie
);

void registerJwksRoute(
	AuthApp &app,
	const auth::RsaTokenSigner &tokenSigner
);