#include <nlohmann/json.hpp>
#include <jwt-cpp/base.h>

#include <unordered_set>

#include <auth/jwks/JsonJwksParser.hpp>

using json = nlohmann::json;

namespace
{
	bool	isValidBase64Url(const std::string &value)
	{
		if (value.empty() || value.size() % 4 == 1)
			return false;
		for (const char c : value)
		{
			const bool isUppercase = c >= 'A' && c <= 'Z';
			const bool isLowerCase = c >= 'a' && c <= 'z';
			const bool isDigit = c >= '0' && c <= '9';
			const bool isSpecial = c == '-' || c == '_';
			if (!isUppercase &&
				!isLowerCase &&
				!isDigit &&
				!isSpecial)
				return false;
		}
		return true;
	}
}

auth::JwksParseResult	auth::JsonJwksParser::parse(
	const std::string &jsonBody
)
{
	try
	{
		const auto document = json::parse(jsonBody);
		const auto &keys = document.at("keys");
		if (!keys.is_array() || keys.empty())
			return {
				JwksParseResult::Status::InvalidJwks,
				{}
			};
		std::vector<auth::Jwk> result;
		std::unordered_set<std::string> keyIds;
		for (const auto &key : keys)
		{
			if (!key.is_object())
				return {
					JwksParseResult::Status::InvalidJwks,
					{}
				};
			const auto kty = key.at("kty").get<std::string>();
			const auto alg = key.contains("alg")
				? key.at("alg").get<std::string>()
				: "RS256";
			if (kty != "RSA" || alg != "RS256")
				continue;
			if (key.contains("use"))
			{
				const auto use = key.at("use").get<std::string>();
				if (use != "sig")
					continue;
			}
			if (key.contains("key_ops"))
			{
				const auto &keyOperations = key.at("key_ops");
				if (!keyOperations.is_array())
					return {
						JwksParseResult::Status::InvalidJwks,
						{}
					};
				bool canVerify = false;
				for (const auto &operation : keyOperations)
				{
					if (!operation.is_string())
						return {
							JwksParseResult::Status::InvalidJwks,
							{}
						};
					if (operation.get<std::string>() == "verify")
						canVerify = true;
				}
				if (!canVerify)
					continue;
			}

			const auto kid = key.at("kid").get<std::string>();
			const auto n = key.at("n").get<std::string>();
			const auto e = key.at("e").get<std::string>();
			if (kid.empty() ||
				!keyIds.insert(kid).second ||
				n.empty() ||
				e.empty() ||
				!isValidBase64Url(n) ||
				!isValidBase64Url(e))
				return {
					JwksParseResult::Status::InvalidJwks,
					{}
				};
			const auto modulusBytes = 
				jwt::base::decode<jwt::alphabet::base64url>(
					jwt::base::pad<jwt::alphabet::base64url>(n)
				);
			if (modulusBytes.size() < 256)
				return {
					JwksParseResult::Status::InvalidJwks,
					{}
				};
			auth::Jwk jwk{
				kid,
				kty,
				alg,
				n,
				e
			};
			result.push_back(std::move(jwk));
		}
		if (result.empty())
			return {
				JwksParseResult::Status::InvalidJwks,
				{}
			};
		return {
			auth::JwksParseResult::Status::Success,
			result
		};
	} catch (const json::exception &)
	{
		return {
			JwksParseResult::Status::InvalidJwks,
			{}
		};
	}
}