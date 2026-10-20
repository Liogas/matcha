#include "AuthRoutes.hpp"
#include "AuthRouteRegistrations.hpp"

void registerAuthRoutes(
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
)
{
	registerEmailAuthRoutes(
		app, accountRepository, passwordHasher, emailSender,
		emailVerificationUrl
	);
	registerPasswordResetRoutes(
		app, accountRepository, passwordHasher, emailSender,
		passwordResetUrl, secureRefreshCookie
	);
	registerSessionAuthRoutes(
		app, authService, authenticator, tokenGenerator,
		loginAttemptLimiter, accountRepository, allowedBrowserOrigin,
		secureRefreshCookie
	);
}

void registerJwksRoute(
	AuthApp &app,
	const auth::RsaTokenSigner &tokenSigner
)
{
	const auto jwk = tokenSigner.publicJwk();
	CROW_ROUTE(app, "/.well-known/jwks.json")
	([jwk]
	{
		crow::json::wvalue body;
		body["keys"][0]["kty"] = jwk.kty;
		body["keys"][0]["kid"] = jwk.kid;
		body["keys"][0]["alg"] = jwk.alg;
		body["keys"][0]["use"] = "sig";
		body["keys"][0]["n"] = jwk.n;
		body["keys"][0]["e"] = jwk.e;
		return crow::response(200, body);
	});
}
