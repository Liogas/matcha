#include "AuthRoutes.hpp"

namespace
{
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
}

void	registerAuthRoutes(
	crow::SimpleApp &app,
	auth::AuthService &authService
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
				return jsonError(
					400,
					"Email and password must be strings"
				);
			LoginRequest request{
				body["email"].s(),
				body["password"].s()
			};
			const auto result = authService.login(
				request.email,
				request.password
			);
			switch (result.status)
			{
				case auth::LoginResult::Status::Success:
				{
					crow::json::wvalue bodyA;
					bodyA["message"] = "Login successful";
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
}
