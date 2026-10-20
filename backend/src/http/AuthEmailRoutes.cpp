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
crow::response handleEmailVerificationPage(const crow::request &req) {
	const char *token = req.url_params.get("token");
	if (token == nullptr)
		return jsonError(400, "Missing verification token");

	try {
		hashEmailVerificationToken(token);
	} catch (const std::invalid_argument &) {
		return jsonError(400, "Invalid verification token");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Verification token hashing failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	crow::response response(
		200, "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
			 "<title>Verify your Matcha email</title>"
			 "<h1>Verify your email</h1>"
			 "<p>Confirm to activate your Matcha account.</p>"
			 "<form method=\"post\" action=\"/api/auth/email/verify\">"
			 "<input type=\"hidden\" name=\"token\" value=\"" +
				 std::string(token) +
				 "\">"
				 "<button type=\"submit\">Verify email</button></form>"
				 "</html>");
	response.set_header("Content-Type", "text/html; charset=utf-8");
	return response;
}

crow::response handleVerifyEmailToken(const crow::request &req,
									  AccountRepository &accountRepository) {
	if (req.body.size() != 70 || req.body.compare(0, 6, "token=") != 0)
		return jsonError(400, "Invalid verification request");
	const std::string token = req.body.substr(6);

	std::string tokenHash;
	try {
		tokenHash = hashEmailVerificationToken(token);
	} catch (const std::invalid_argument &) {
		return jsonError(400, "Invalid verification token");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Verification token hashing failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto result = accountRepository.verifyEmailToken(tokenHash);
	if (result.status == EmailVerificationResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Email verification failed" << std::endl;
		return jsonError(500, "Internal server error");
	}
	if (result.status == EmailVerificationResult::Status::InvalidOrExpired)
		return jsonError(
			400, "Verification token is invalid, expired, or already used");
	return jsonMessage(200, "Email verified successfully");
}

crow::response handleRegisterAccount(const crow::request &req,
									 AccountRepository &accountRepository,
									 auth::PasswordHasher &passwordHasher,
									 const SmtpEmailSender &emailSender,
									 const std::string &emailVerificationUrl
) {
	const auto body = crow::json::load(req.body);
	if (!body)
		return jsonError(400, "Invalid JSON");
	if (!body.has("email") || !body.has("username") ||
		!body.has("first_name") || !body.has("last_name") ||
		!body.has("password"))
		return jsonError(400, "Missing fields");
	if (body["email"].t() != crow::json::type::String ||
		body["username"].t() != crow::json::type::String ||
		body["first_name"].t() != crow::json::type::String ||
		body["last_name"].t() != crow::json::type::String ||
		body["password"].t() != crow::json::type::String) {
		return jsonError(400, "All registration fields must be strings");
	}

	RegisterRequest request{lowercaseAscii(trim(body["email"].s())),
							trim(body["username"].s()),
							trim(body["first_name"].s()),
							trim(body["last_name"].s()), body["password"].s()};
	if (!validEmail(request.email))
		return jsonError(400, "Invalid email");
	if (!validUsername(request.username))
		return jsonError(
			400, "Username must be 3-30 ASCII letters, digits, or underscores");
	if (!validPersonName(request.firstName) ||
		!validPersonName(request.lastName))
		return jsonError(400,
						 "First and last names must be 1-80 valid characters");
	if (!validPassword(request.password))
		return jsonError(
			400,
			"Password must be 12-128 characters and not a common password");

	std::string passwordHash;
	try {
		passwordHash = passwordHasher.hash(request.password);
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Password hashing failed: " << error.what()
				  << std::endl;
		return jsonError(500, "Internal server error");
	}

	EmailVerificationToken verificationToken;
	try {
		verificationToken = generateEmailVerificationToken();
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Verification token generation failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto result = accountRepository.createPendingAccountAndProfile(
		request.email, passwordHash, request.username, request.firstName,
		request.lastName, verificationToken.hash, std::chrono::hours{24});
	if (result.status == AccountCreationResult::Status::IdentifierAlreadyExists)
		return jsonError(409, "Email or username already in use");
	if (result.status == AccountCreationResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Account/profile creation failed"
				  << std::endl;
		return jsonError(500, "Internal server error");
	}

	try {
		emailSender.sendTextEmail(
			request.email, "Verify your Matcha email",
			"Welcome to Matcha. Verify your email within 24 hours using "
			"this link:\r\n\r\n" +
				emailVerificationUrl + "?token=" + verificationToken.value +
				"\r\n");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Verification email delivery failed: "
				  << error.what() << std::endl;
		return jsonError(
			503, "Account created, but verification email delivery failed. "
				 "Please try again later.");
	}

	return jsonMessage(
		201, "Account created. Check your email to verify your account.");
}

crow::response
handleResendVerificationEmail(const crow::request &req,
							  AccountRepository &accountRepository,
							  const SmtpEmailSender &emailSender,
							  const std::string &emailVerificationUrl
) {
	const auto body = crow::json::load(req.body);
	if (!body || !body.has("email") ||
		body["email"].t() != crow::json::type::String)
		return jsonError(400, "A valid email is required");

	const std::string email = lowercaseAscii(trim(body["email"].s()));
	if (!validEmail(email))
		return jsonError(400, "A valid email is required");

	EmailVerificationToken token;
	try {
		token = generateEmailVerificationToken();
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Verification token generation failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto result = accountRepository.issueEmailVerificationToken(
		email, token.hash, std::chrono::hours{24});
	if (result.status == ResendVerificationResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Verification token resend failed"
				  << std::endl;
		return jsonError(500, "Internal server error");
	}
	if (result.status == ResendVerificationResult::Status::Issued) {
		try {
			emailSender.sendTextEmail(
				result.email, "Verify your Matcha email",
				"Verify your email within 24 hours using this link:\r\n\r\n" +
					emailVerificationUrl + "?token=" + token.value + "\r\n");
		} catch (const std::exception &error) {
			std::cerr << "[AUTH ERROR] Verification email resend failed: "
					  << error.what() << std::endl;
			return jsonError(503, "Unable to process the request right now");
		}
	}

	return jsonMessage(
		202,
		"If an unverified account exists, a verification email has been sent.");
}

} // namespace

void registerEmailAuthRoutes(AuthApp &app, AccountRepository &accountRepository,
							 auth::PasswordHasher &passwordHasher,
							 const SmtpEmailSender &emailSender,
							 const std::string &emailVerificationUrl) {
	CROW_ROUTE(app, "/api/auth/email/verify")
		.methods(crow::HTTPMethod::GET)([](const crow::request &req) {
			return handleEmailVerificationPage(req);
		});

	CROW_ROUTE(app, "/api/auth/email/verify")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository](const crow::request &req) {
				return handleVerifyEmailToken(req, accountRepository);
			});

	CROW_ROUTE(app, "/api/auth/register")
		.methods(crow::HTTPMethod::POST)([&accountRepository, &passwordHasher,
										  &emailSender, &emailVerificationUrl](
											 const crow::request &req) {
			return handleRegisterAccount(req, accountRepository, passwordHasher,
										 emailSender, emailVerificationUrl);
		});

	CROW_ROUTE(app, "/api/auth/email/resend")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository, &emailSender,
			 &emailVerificationUrl](const crow::request &req) {
				return handleResendVerificationEmail(
					req, accountRepository, emailSender, emailVerificationUrl);
			});
}
