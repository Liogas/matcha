#include "../security/EmailVerificationToken.hpp"
#include "../security/OpaqueToken.hpp"
#include "AuthRouteRegistrations.hpp"
#include "AuthRouteSupport.hpp"

#include <cctype>
#include <charconv>
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sw/redis++/errors.h>
#include <unordered_set>

using namespace auth_routes;

namespace {
crow::response handleLogin(const crow::request &req,
						   auth::AuthService &authService,
						   auth::TokenGenerator &tokenGenerator,
						   RedisLoginAttemptLimiter &loginAttemptLimiter,
						   AccountRepository &accountRepository,
						   bool secureRefreshCookie) {
	const auto body = crow::json::load(req.body);
	if (!body)
		return jsonError(400, "Invalid JSON");
	if (!body.has("email") || !body.has("password"))
		return jsonError(400, "Missing fields");
	if (body["email"].t() != crow::json::type::String ||
		body["password"].t() != crow::json::type::String)
		return jsonError(400, "Email and password must be strings");
	LoginRequest request{lowercaseAscii(trim(body["email"].s())),
						 body["password"].s()};
	const std::string ipAddress = clientIp(req);
	if (ipAddress.empty()) {
		std::cerr << "[AUTH ERROR] Login request has no client IP" << std::endl;
		return jsonError(503, "Login temporarily unavailable");
	}

	LoginAttemptDecision attempt;
	try {
		attempt = loginAttemptLimiter.consume(request.email, ipAddress);
	} catch (const sw::redis::Error &error) {
		std::cerr << "[AUTH ERROR] Login limiter unavailable: " << error.what()
				  << std::endl;
		return jsonError(503, "Login temporarily unavailable");
	} catch (const std::runtime_error &error) {
		std::cerr << "[AUTH ERROR] Login limiter failed: " << error.what()
				  << std::endl;
		return jsonError(503, "Login temporarily unavailable");
	}
	if (!attempt.allowed) {
		auto response = jsonError(429, "Too many login attempts. Retry later.");
		response.add_header("Retry-After",
							std::to_string(attempt.retryAfterSeconds));
		return response;
	}

	const auto result = authService.login(request.email, request.password);
	switch (result.status) {
	case auth::LoginResult::Status::Success: {
		if (!result.user.has_value())
			return jsonError(500, "Internal server error");
		std::string token;
		try {
			token = tokenGenerator.generate(
				{std::to_string(result.user->id), {}, {}});
		} catch (const std::exception &error) {
			std::cerr << "[AUTH ERROR] JWT signing failed: " << error.what()
					  << std::endl;
			return jsonError(500, "Internal server error");
		}
		OpaqueToken refreshToken;
		try {
			refreshToken = generateOpaqueToken();
		} catch (const std::exception &error) {
			std::cerr << "[AUTH ERROR] Refresh token generation failed: "
					  << error.what() << std::endl;
			return jsonError(500, "Internal server error");
		}
		try {
			loginAttemptLimiter.clear(request.email, ipAddress);
		} catch (const sw::redis::Error &error) {
			std::cerr << "[AUTH ERROR] Unable to clear login limiter: "
					  << error.what() << std::endl;
			return jsonError(503, "Login temporarily unavailable");
		}

		const auto session = accountRepository.createSession(
			result.user->id, refreshToken.hash, std::chrono::days{7},
			std::chrono::days{30});
		if (session.status != SessionResult::Status::Success) {
			std::cerr << "[AUTH ERROR] Session creation failed" << std::endl;
			return jsonError(500, "Unable to create session");
		}

		crow::json::wvalue bodyA;
		bodyA["message"] = "Login successful";
		bodyA["access_token"] = token;
		bodyA["token_type"] = "Bearer";
		bodyA["user"]["id"] = result.user->id;
		bodyA["user"]["email"] = result.user->email;
		crow::response response(200, bodyA);
		response.add_header("Set-Cookie", refreshCookie(refreshToken.value,
														secureRefreshCookie,
														std::chrono::days{7}));
		return response;
	}
	case auth::LoginResult::Status::InvalidCredentials:
		return jsonError(401, "Invalid credentials");
	case auth::LoginResult::Status::InternalError:
		return jsonError(500, "Internal server error");
	}
	return jsonError(500, "Unknown authentification result");
}

crow::response handleRefreshSession(const crow::request &req,
									AccountRepository &accountRepository,
									auth::TokenGenerator &tokenGenerator,
									const std::string &allowedBrowserOrigin,
									bool secureRefreshCookie) {
	if (req.get_header_value("Origin") != allowedBrowserOrigin)
		return jsonError(403, "Request origin is not allowed");
	const auto currentToken = requestCookie(req, "matcha_refresh");
	if (!currentToken.has_value())
		return jsonError(401, "Refresh session is missing");

	OpaqueToken replacementToken;
	try {
		replacementToken = generateOpaqueToken();
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Refresh token generation failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	std::string currentTokenHash;
	try {
		currentTokenHash = hashOpaqueToken(*currentToken);
	} catch (const std::invalid_argument &) {
		return jsonError(401, "Invalid refresh session");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Refresh token hashing failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto session = accountRepository.rotateRefreshToken(
		currentTokenHash, replacementToken.hash, std::chrono::days{7});
	if (session.status == SessionResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Session refresh failed" << std::endl;
		return jsonError(500, "Internal server error");
	}
	if (session.status == SessionResult::Status::InvalidToken)
		return jsonError(401, "Refresh session is invalid or expired");

	std::string accessToken;
	try {
		accessToken = tokenGenerator.generate(
			{std::to_string(session.accountId), {}, {}});
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] JWT signing failed: " << error.what()
				  << std::endl;
		return jsonError(500, "Internal server error");
	}

	crow::json::wvalue body;
	body["message"] = "Session refreshed";
	body["access_token"] = accessToken;
	body["token_type"] = "Bearer";
	crow::response response(200, body);
	response.add_header("Set-Cookie", refreshCookie(replacementToken.value,
													secureRefreshCookie,
													std::chrono::days{7}));
	return response;
}

crow::response handleLogoutSession(const crow::request &req,
								   AccountRepository &accountRepository,
								   const std::string &allowedBrowserOrigin,
								   bool secureRefreshCookie) {
	if (req.get_header_value("Origin") != allowedBrowserOrigin)
		return jsonError(403, "Request origin is not allowed");
	const auto token = requestCookie(req, "matcha_refresh");
	if (token.has_value()) {
		try {
			if (!accountRepository.revokeSessionByRefreshToken(
					hashOpaqueToken(*token)))
				return jsonError(500, "Unable to revoke session");
		} catch (const std::invalid_argument &) {
		} catch (const std::exception &error) {
			std::cerr << "[AUTH ERROR] Session logout failed: " << error.what()
					  << std::endl;
			return jsonError(500, "Internal server error");
		}
	}
	crow::response response(200, "Logged out");
	response.add_header(
		"Set-Cookie",
		refreshCookie("", secureRefreshCookie, std::chrono::seconds::zero()));
	return response;
}

crow::response handleLogoutAllSessions(const crow::request &req,
									   AccountRepository &accountRepository,
									   auth::Authenticator &authenticator,
									   bool secureRefreshCookie) {
	const auto token = bearerToken(req);
	if (!token.has_value())
		return jsonError(401, "Missing or invalid bearer token");
	const auto authResult = authenticator.authenticate(*token);
	if (authResult.status ==
		auth::TokenValidationResult::Status::VerificationUnavailable)
		return jsonError(503, "Token verification temporarily unavailable");
	if (authResult.status != auth::TokenValidationResult::Status::Valid ||
		!authResult.claims.has_value())
		return jsonError(401, "Invalid token");

	std::int64_t accountId = 0;
	const auto &subject = authResult.claims->subject;
	const auto parsed = std::from_chars(
		subject.data(), subject.data() + subject.size(), accountId);
	if (parsed.ec != std::errc{} ||
		parsed.ptr != subject.data() + subject.size() || accountId <= 0)
		return jsonError(401, "Invalid token subject");
	if (!accountRepository.revokeAllSessions(accountId)) {
		std::cerr << "[AUTH ERROR] Global session revocation failed"
				  << std::endl;
		return jsonError(500, "Internal server error");
	}
	crow::response response(200, "Logged out from all sessions");
	response.add_header(
		"Set-Cookie",
		refreshCookie("", secureRefreshCookie, std::chrono::seconds::zero()));
	return response;
}

crow::response handleGetCurrentUser(const crow::request &req,
									auth::Authenticator &authenticator) {
	const auto token = bearerToken(req);
	if (!token.has_value())
		return jsonError(401, "Missing or invalid bearer token");

	const auto result = authenticator.authenticate(*token);
	if (result.status ==
		auth::TokenValidationResult::Status::VerificationUnavailable)
		return jsonError(503, "Token verification temporarily unavailable");
	if (result.status != auth::TokenValidationResult::Status::Valid)
		return jsonError(401, "Invalid token");
	if (!result.claims.has_value())
		return jsonError(500, "Internal server error");

	crow::json::wvalue body;
	body["user"]["id"] = result.claims->subject;
	body["issuer"] = result.claims->issuer;
	body["audience"] = result.claims->audience;
	return crow::response(200, body);
}

} // namespace

void registerSessionAuthRoutes(AuthApp &app, auth::AuthService &authService,
							   auth::Authenticator &authenticator,
							   auth::TokenGenerator &tokenGenerator,
							   RedisLoginAttemptLimiter &loginAttemptLimiter,
							   AccountRepository &accountRepository,
							   const std::string &allowedBrowserOrigin,
							   bool secureRefreshCookie) {
	CROW_ROUTE(app, "/api/auth/login")
		.methods(crow::HTTPMethod::POST)(
			[&authService, &tokenGenerator, &loginAttemptLimiter,
			 &accountRepository,
			 secureRefreshCookie](const crow::request &req) {
				return handleLogin(req, authService, tokenGenerator,
								   loginAttemptLimiter, accountRepository,
								   secureRefreshCookie);
			});

	CROW_ROUTE(app, "/api/auth/refresh")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository, &tokenGenerator, allowedBrowserOrigin,
			 secureRefreshCookie](const crow::request &req) {
				return handleRefreshSession(
					req, accountRepository, tokenGenerator,
					allowedBrowserOrigin, secureRefreshCookie);
			});

	CROW_ROUTE(app, "/api/auth/logout")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository, allowedBrowserOrigin,
			 secureRefreshCookie](const crow::request &req) {
				return handleLogoutSession(req, accountRepository,
										   allowedBrowserOrigin,
										   secureRefreshCookie);
			});

	CROW_ROUTE(app, "/api/auth/logout-all")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository, &authenticator,
			 secureRefreshCookie](const crow::request &req) {
				return handleLogoutAllSessions(
					req, accountRepository, authenticator, secureRefreshCookie);
			});

	CROW_ROUTE(app, "/api/auth/me")
		.methods(crow::HTTPMethod::GET)(
			[&authenticator](const crow::request &req) {
				return handleGetCurrentUser(req, authenticator);
			});
}
