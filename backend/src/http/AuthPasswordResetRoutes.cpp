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
crow::response handleRequestPasswordReset(const crow::request &req,
										  AccountRepository &accountRepository,
										  const SmtpEmailSender &emailSender,
										  const std::string &passwordResetUrl) {
	const auto body = crow::json::load(req.body);
	if (!body || !body.has("email") ||
		body["email"].t() != crow::json::type::String)
		return jsonError(400, "A valid email is required");

	const std::string email = lowercaseAscii(trim(body["email"].s()));
	if (!validEmail(email))
		return jsonError(400, "A valid email is required");

	OpaqueToken token;
	try {
		token = generateOpaqueToken();
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Password reset token generation failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto result = accountRepository.issuePasswordResetToken(
		email, token.hash, std::chrono::minutes{30});
	if (result.status == PasswordResetRequestResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Password reset request failed" << std::endl;
		return jsonError(500, "Internal server error");
	}
	if (result.status == PasswordResetRequestResult::Status::Issued) {
		try {
			emailSender.sendTextEmail(
				result.email, "Reset your Matcha password",
				"Use this link within 30 minutes to reset your "
				"password:\r\n\r\n" +
					passwordResetUrl + "?token=" + token.value + "\r\n");
		} catch (const std::exception &error) {
			std::cerr << "[AUTH ERROR] Password reset email delivery failed: "
					  << error.what() << std::endl;
			return jsonError(503, "Unable to process the request right now");
		}
	}

	return jsonMessage(202, "If an account exists for this address, password "
							"reset instructions have been sent.");
}

crow::response handlePasswordResetPage(const crow::request &req) {
	const char *token = req.url_params.get("token");
	if (token == nullptr)
		return jsonError(400, "Missing password reset token");
	try {
		hashOpaqueToken(token);
	} catch (const std::invalid_argument &) {
		return jsonError(400, "Invalid password reset token");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Password reset token validation failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	crow::response response(
		200,
		"<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
		"<meta name=\"referrer\" content=\"no-referrer\">"
		"<title>Reset your Matcha password</title>"
		"<h1>Reset your password</h1>"
		"<form id=\"reset-form\">"
		"<input type=\"hidden\" id=\"token\" value=\"" +
			std::string(token) +
			"\">"
			"<label>New password "
			"<input id=\"password\" type=\"password\" minlength=\"12\" "
			"maxlength=\"128\" required autocomplete=\"new-password\"></label>"
			"<button type=\"submit\">Reset password</button></form>"
			"<p id=\"result\" role=\"status\"></p>"
			"<script>"
			"document.getElementById('reset-form').addEventListener('submit',"
			"async event => {event.preventDefault();"
			"const result = document.getElementById('result');"
			"try { const response = await fetch('', {method:'POST',"
			"headers:{'Content-Type':'application/json'},"
			"body:JSON.stringify({token:document.getElementById('token').value,"
			"password:document.getElementById('password').value})});"
			"const body = await response.json();"
			"result.textContent = response.ok ? body.message : body.error;"
			"} catch (_) { result.textContent = 'Unable to reset password. Try "
			"again later.'; }"
			"});</script></html>");
	response.set_header("Content-Type", "text/html; charset=utf-8");
	response.set_header("Cache-Control", "no-store");
	response.set_header("Referrer-Policy", "no-referrer");
	return response;
}

crow::response handleConfirmPasswordReset(const crow::request &req,
										  AccountRepository &accountRepository,
										  auth::PasswordHasher &passwordHasher,
										  bool secureRefreshCookie) {
	const auto body = crow::json::load(req.body);
	if (!body || !body.has("token") || !body.has("password") ||
		body["token"].t() != crow::json::type::String ||
		body["password"].t() != crow::json::type::String)
		return jsonError(400, "A valid token and password are required");

	const std::string &token = body["token"].s();
	const std::string &password = body["password"].s();
	if (!validPassword(password))
		return jsonError(
			400,
			"Password must be 12-128 characters and not a common password");

	std::string tokenHash;
	try {
		tokenHash = hashOpaqueToken(token);
	} catch (const std::invalid_argument &) {
		return jsonError(400, "Invalid password reset token");
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Password reset token hashing failed: "
				  << error.what() << std::endl;
		return jsonError(500, "Internal server error");
	}

	std::string passwordHash;
	try {
		passwordHash = passwordHasher.hash(password);
	} catch (const std::exception &error) {
		std::cerr << "[AUTH ERROR] Password hashing failed: " << error.what()
				  << std::endl;
		return jsonError(500, "Internal server error");
	}

	const auto result =
		accountRepository.resetPassword(tokenHash, passwordHash);
	if (result.status == PasswordResetResult::Status::DatabaseError) {
		std::cerr << "[AUTH ERROR] Password reset failed" << std::endl;
		return jsonError(500, "Internal server error");
	}
	if (result.status == PasswordResetResult::Status::InvalidOrExpired)
		return jsonError(
			400, "Password reset token is invalid, expired, or already used");

	crow::response response =
		jsonMessage(200, "Password reset successfully. Please sign in again.");
	response.add_header(
		"Set-Cookie",
		refreshCookie("", secureRefreshCookie, std::chrono::seconds::zero()));
	return response;
}

} // namespace

void registerPasswordResetRoutes(AuthApp &app,
								 AccountRepository &accountRepository,
								 auth::PasswordHasher &passwordHasher,
								 const SmtpEmailSender &emailSender,
								 const std::string &passwordResetUrl,
								 bool secureRefreshCookie) {
	CROW_ROUTE(app, "/api/auth/password-reset/request")
		.methods(crow::HTTPMethod::POST)(
			[&accountRepository, &emailSender,
			 &passwordResetUrl](const crow::request &req) {
				return handleRequestPasswordReset(
					req, accountRepository, emailSender, passwordResetUrl);
			});

	CROW_ROUTE(app, "/api/auth/password-reset/confirm")
		.methods(crow::HTTPMethod::GET)([](const crow::request &req) {
			return handlePasswordResetPage(req);
		});

	CROW_ROUTE(app, "/api/auth/password-reset/confirm")
		.methods(crow::HTTPMethod::POST)([&accountRepository, &passwordHasher,
										  secureRefreshCookie](
											 const crow::request &req) {
			return handleConfirmPasswordReset(
				req, accountRepository, passwordHasher, secureRefreshCookie);
		});
}
