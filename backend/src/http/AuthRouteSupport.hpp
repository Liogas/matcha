#pragma once

#include <crow.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace auth {
class Authenticator;
}

namespace auth_routes {
std::string trim(const std::string &value);
std::optional<std::size_t> utf8CharacterCount(const std::string &value);
std::string lowercaseAscii(std::string value);
bool validEmail(const std::string &email);
bool validUsername(const std::string &username);
bool validPersonName(const std::string &name);
bool validPassword(const std::string &password);
crow::response jsonError(int status, const std::string &message);
crow::response jsonMessage(int status, const std::string &message);
std::optional<std::string> bearerToken(const crow::request &request);
std::optional<std::int64_t> authenticatedAccountId(
	const crow::request &request, auth::Authenticator &authenticator);
std::string clientIp(const crow::request &request);
std::string refreshCookie(const std::string &value, bool secure,
						  std::chrono::seconds maxAge);
std::optional<std::string> requestCookie(const crow::request &request,
										 std::string_view name);
} // namespace auth_routes
