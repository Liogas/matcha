#include "AuthRouteSupport.hpp"

#include <auth/auth/Authenticator.hpp>
#include <cctype>
#include <charconv>
#include <unordered_set>

namespace auth_routes {

constexpr std::string_view BEARER_PREFIX = "Bearer ";
const std::unordered_set<std::string> COMMON_PASSWORDS{
	"123456789012", "adminadmin12", "iloveyou1234", "letmein12345",
	"password1234", "qwerty123456", "welcome12345"};

std::string trim(const std::string &value) {
	const auto first = value.find_first_not_of(" \t\n\r\f\v");
	if (first == std::string::npos)
		return {};
	const auto last = value.find_last_not_of(" \t\n\r\f\v");
	return value.substr(first, last - first + 1);
}

std::optional<std::size_t> utf8CharacterCount(const std::string &value) {
	std::size_t count = 0;
	for (std::size_t index = 0; index < value.size();) {
		const auto lead = static_cast<unsigned char>(value[index]);
		std::size_t width = 0;
		if (lead <= 0x7f)
			width = 1;
		else if (lead >= 0xc2 && lead <= 0xdf)
			width = 2;
		else if (lead >= 0xe0 && lead <= 0xef)
			width = 3;
		else if (lead >= 0xf0 && lead <= 0xf4)
			width = 4;
		else
			return std::nullopt;

		if (index + width > value.size())
			return std::nullopt;
		for (std::size_t offset = 1; offset < width; ++offset) {
			const auto continuation =
				static_cast<unsigned char>(value[index + offset]);
			if (continuation < 0x80 || continuation > 0xbf)
				return std::nullopt;
		}
		if (width == 3) {
			const auto second = static_cast<unsigned char>(value[index + 1]);
			if ((lead == 0xe0 && second < 0xa0) ||
				(lead == 0xed && second > 0x9f))
				return std::nullopt;
		}
		if (width == 4) {
			const auto second = static_cast<unsigned char>(value[index + 1]);
			if ((lead == 0xf0 && second < 0x90) ||
				(lead == 0xf4 && second > 0x8f))
				return std::nullopt;
		}
		index += width;
		++count;
	}
	return count;
}

std::string lowercaseAscii(std::string value) {
	for (char &character : value) {
		const auto byte = static_cast<unsigned char>(character);
		if (byte >= 'A' && byte <= 'Z')
			character = static_cast<char>(byte - 'A' + 'a');
	}
	return value;
}

bool validEmail(const std::string &email) {
	if (email.size() < 3 || email.size() > 254 ||
		email.find_first_of(" \t\r\n") != std::string::npos)
		return false;
	const auto at = email.find('@');
	if (at == std::string::npos || at == 0 || at != email.rfind('@') ||
		at + 1 >= email.size())
		return false;
	const auto dot = email.find('.', at + 2);
	return dot != std::string::npos && dot + 1 < email.size();
}

bool validUsername(const std::string &username) {
	if (username.size() < 3 || username.size() > 30)
		return false;
	for (const unsigned char character : username) {
		if (!((character >= 'a' && character <= 'z') ||
			  (character >= 'A' && character <= 'Z') ||
			  (character >= '0' && character <= '9') || character == '_'))
			return false;
	}
	return true;
}

bool validPersonName(const std::string &name) {
	const auto count = utf8CharacterCount(name);
	if (!count.has_value() || *count == 0 || *count > 80)
		return false;
	for (const unsigned char character : name) {
		if (character < 0x20 || character == 0x7f)
			return false;
	}
	return true;
}

bool validPassword(const std::string &password) {
	const auto count = utf8CharacterCount(password);
	if (!count.has_value() || *count < 12 || *count > 128)
		return false;
	return !COMMON_PASSWORDS.contains(lowercaseAscii(password));
}

crow::response jsonError(int status, const std::string &message) {
	crow::json::wvalue body;
	body["error"] = message;
	return crow::response(status, body);
}

crow::response jsonMessage(int status, const std::string &message) {
	crow::json::wvalue body;
	body["message"] = message;
	return crow::response(status, body);
}

std::optional<std::string> bearerToken(const crow::request &request) {
	const std::string authorization = request.get_header_value("Authorization");
	if (authorization.size() <= BEARER_PREFIX.size())
		return std::nullopt;

	for (std::size_t index = 0; index < BEARER_PREFIX.size(); ++index) {
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

std::optional<std::int64_t> authenticatedAccountId(
	const crow::request &request, auth::Authenticator &authenticator) {
	const auto token = bearerToken(request);
	if (!token)
		return std::nullopt;
	const auto result = authenticator.authenticate(*token);
	if (result.status != auth::TokenValidationResult::Status::Valid ||
		!result.claims)
		return std::nullopt;
	std::int64_t id = 0;
	const auto &subject = result.claims->subject;
	const auto parsed = std::from_chars(
		subject.data(), subject.data() + subject.size(), id);
	if (parsed.ec != std::errc{} ||
		parsed.ptr != subject.data() + subject.size() || id <= 0)
		return std::nullopt;
	return id;
}

std::string clientIp(const crow::request &request) {
	const std::string proxyIp = request.get_header_value("X-Real-IP");
	return proxyIp.empty() ? request.remote_ip_address : proxyIp;
}

std::string refreshCookie(const std::string &value, bool secure,
						  std::chrono::seconds maxAge) {
	std::string cookie = "matcha_refresh=" + value +
						 "; Max-Age=" + std::to_string(maxAge.count()) +
						 "; Path=/api/auth; HttpOnly; SameSite=Strict";
	if (secure)
		cookie += "; Secure";
	return cookie;
}

std::optional<std::string> requestCookie(const crow::request &request,
										 std::string_view name) {
	const std::string header = request.get_header_value("Cookie");
	const std::string prefix = std::string(name) + "=";
	std::optional<std::string> value;
	std::size_t segmentStart = 0;
	while (segmentStart < header.size()) {
		const auto segmentEnd = header.find(';', segmentStart);
		const auto end =
			segmentEnd == std::string::npos ? header.size() : segmentEnd;
		auto begin = segmentStart;
		while (begin < end &&
			   std::isspace(static_cast<unsigned char>(header[begin])))
			++begin;
		auto trimmedEnd = end;
		while (trimmedEnd > begin &&
			   std::isspace(static_cast<unsigned char>(header[trimmedEnd - 1])))
			--trimmedEnd;
		if (header.compare(begin, prefix.size(), prefix) == 0) {
			if (value.has_value())
				return std::nullopt;
			value = header.substr(begin + prefix.size(),
								  trimmedEnd - begin - prefix.size());
		}
		if (segmentEnd == std::string::npos)
			break;
		segmentStart = segmentEnd + 1;
	}
	return value;
}
} // namespace auth_routes
