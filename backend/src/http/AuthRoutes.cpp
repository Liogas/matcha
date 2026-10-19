#include "AuthRoutes.hpp"

#include <cctype>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sw/redis++/errors.h>

namespace
{
	constexpr std::string_view BEARER_PREFIX = "Bearer ";

	crow::response jsonError(
		int status,
		const std::string &message
	)
	{
		crow::json::wvalue body;
		body["error"] = message;
		return crow::response(status, body);
	}

	crow::response jsonMessage(
		int status,
		const std::string &message
	)
	{
		crow::json::wvalue body;
		body["message"] = message;
		return crow::response(status, body);
	}

	std::optional<std::string> bearerToken(const crow::request &request)
	{
		const std::string authorization =
			request.get_header_value("Authorization");
		if (authorization.size() <= BEARER_PREFIX.size())
			return std::nullopt;

		for (std::size_t index = 0; index < BEARER_PREFIX.size(); ++index)
		{
			const auto character = static_cast<unsigned char>(authorization[index]);
			if (std::tolower(character) !=
				std::tolower(static_cast<unsigned char>(BEARER_PREFIX[index])))
				return std::nullopt;
		}

		const std::string token = authorization.substr(BEARER_PREFIX.size());
		if (token.find_first_of(" \t\r\n") != std::string::npos)
			return std::nullopt;
		return token;
	}

	std::string clientIp(const crow::request &request)
	{
		const std::string proxyIp =
			request.get_header_value("X-Real-IP");
		return proxyIp.empty() ? request.remote_ip_address : proxyIp;
	}
}

void	registerAuthRoutes(
	crow::SimpleApp &app,
	auth::AuthService &authService,
	auth::Authenticator	&authenticator,
	auth::TokenGenerator &tokenGenerator,
	RedisLoginAttemptLimiter &loginAttemptLimiter
)
{
	CROW_ROUTE(app, "/api/auth/register")
		.methods(crow::HTTPMethod::POST)
	(
		[&authService](const crow::request &req)
		{
			const auto body = crow::json::load(req.body);
			if (!body)
				return jsonError(
					400,
					"Invalid JSON"
				);
			if (!body.has("email") || !body.has("password"))
				return jsonError(
					400,
					"Missing fields"
				);
			if (body["email"].t() != crow::json::type::String ||
				body["password"].t() != crow::json::type::String)
			{
				return jsonError(
					400,
					"Email and password must be strings"
				);
			}

			RegisterRequest request{
				body["email"].s(),
				body["password"].s()
			};
			const auto result = authService.registerUser(
				request.email,
				request.password
			);
			switch (result)
			{
				case auth::RegisterResult::Success:
					return jsonMessage(201, "User registered successfully");
				case auth::RegisterResult::EmailAlreadyExists:
					return jsonError(409, "Email already exists");
				case auth::RegisterResult::InvalidEmail:
					return jsonError(400, "Invalid email");
				case auth::RegisterResult::InvalidPassword:
					return jsonError(400, "Invalid password");
				case auth::RegisterResult::InternalError:
					return jsonError(500, "Internal server error");
				default:
					return jsonError(500, "Unknown register result");
			}
		}
	);

	CROW_ROUTE(app, "/api/auth/login")
		.methods(crow::HTTPMethod::POST)
	(
		[&authService, &tokenGenerator, &loginAttemptLimiter](
			const crow::request &req
		)
		{
			const auto body = crow::json::load(req.body);
			if (!body)
				return jsonError(
					400,
					"Invalid JSON"
				);
			if (!body.has("email") || !body.has("password"))
				return jsonError(
					400,
					"Missing fields"
				);
			if (body["email"].t() != crow::json::type::String ||
				body["password"].t() != crow::json::type::String)
				return jsonError(
					400,
					"Email and password must be strings"
				);
			LoginRequest request{
				body["email"].s(),
				body["password"].s()
			};
			const std::string ipAddress = clientIp(req);
			if (ipAddress.empty())
			{
				std::cerr << "[AUTH ERROR] Login request has no client IP"
					<< std::endl;
				return jsonError(
					503,
					"Login temporarily unavailable"
				);
			}

			LoginAttemptDecision attempt;
			try
			{
				attempt = loginAttemptLimiter.consume(
					request.email,
					ipAddress
				);
			}
			catch (const sw::redis::Error &error)
			{
				std::cerr << "[AUTH ERROR] Login limiter unavailable: "
					<< error.what() << std::endl;
				return jsonError(
					503,
					"Login temporarily unavailable"
				);
			}
			catch (const std::runtime_error &error)
			{
				std::cerr << "[AUTH ERROR] Login limiter failed: "
					<< error.what() << std::endl;
				return jsonError(
					503,
					"Login temporarily unavailable"
				);
			}
			if (!attempt.allowed)
			{
				auto response = jsonError(
					429,
					"Too many login attempts. Retry later."
				);
				response.add_header(
					"Retry-After",
					std::to_string(attempt.retryAfterSeconds)
				);
				return response;
			}

			const auto result = authService.login(
				request.email,
				request.password
			);
			switch (result.status)
			{
				case auth::LoginResult::Status::Success:
				{
					if (!result.user.has_value())
						return jsonError(
							500,
							"Internal server error"
						);
					std::string token;
					try
					{
						token = tokenGenerator.generate({
							std::to_string(result.user->id),
							{},
							{}
						});
					}
					catch (const std::exception &error)
					{
						std::cerr << "[AUTH ERROR] JWT signing failed: "
							<< error.what() << std::endl;
						return jsonError(
							500,
							"Internal server error"
						);
					}
					try
					{
						loginAttemptLimiter.clear(
							request.email,
							ipAddress
						);
					}
					catch (const sw::redis::Error &error)
					{
						std::cerr
							<< "[AUTH ERROR] Unable to clear login limiter: "
							<< error.what() << std::endl;
						return jsonError(
							503,
							"Login temporarily unavailable"
						);
					}

					crow::json::wvalue bodyA;
					bodyA["message"] = "Login successful";
					bodyA["access_token"] = token;
					bodyA["token_type"] = "Bearer";
					bodyA["user"]["id"] = result.user->id;
					bodyA["user"]["email"] = result.user->email;
					return crow::response(
						200,
						bodyA
					);
				}
				case auth::LoginResult::Status::InvalidCredentials:
					return jsonError(
						401,
						"Invalid credentials"
					);
				case auth::LoginResult::Status::InternalError:
					return jsonError(
						500,
						"Internal server error"
					);
			}
			return jsonError(
				500,
				"Unknown authentification result"
			);
		}
	);

	CROW_ROUTE(app, "/api/auth/me")
		.methods(crow::HTTPMethod::GET)
	(
		[&authenticator](const crow::request &req)
		{
			const auto token = bearerToken(req);
			if (!token.has_value())
				return jsonError(401, "Missing or invalid bearer token");

			const auto result = authenticator.authenticate(*token);
			if (result.status ==
				auth::TokenValidationResult::Status::VerificationUnavailable)
				return jsonError(
					503,
					"Token verification temporarily unavailable"
				);
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
	);
}

void registerJwksRoute(
	crow::SimpleApp &app,
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
