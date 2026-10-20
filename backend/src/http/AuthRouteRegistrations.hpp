#pragma once

#include "AuthRoutes.hpp"

void registerEmailAuthRoutes(AuthApp &app, AccountRepository &accountRepository,
							 auth::PasswordHasher &passwordHasher,
							 const SmtpEmailSender &emailSender,
							 const std::string &emailVerificationUrl);
void registerPasswordResetRoutes(AuthApp &app,
								 AccountRepository &accountRepository,
								 auth::PasswordHasher &passwordHasher,
								 const SmtpEmailSender &emailSender,
								 const std::string &passwordResetUrl,
								 bool secureRefreshCookie);
void registerSessionAuthRoutes(AuthApp &app, auth::AuthService &authService,
							   auth::Authenticator &authenticator,
							   auth::TokenGenerator &tokenGenerator,
							   RedisLoginAttemptLimiter &loginAttemptLimiter,
							   AccountRepository &accountRepository,
							   const std::string &allowedBrowserOrigin,
							   bool secureRefreshCookie);
