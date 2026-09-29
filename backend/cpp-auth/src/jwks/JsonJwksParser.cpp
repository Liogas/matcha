#include <auth/jwks/JsonJwksParser.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

auth::JwksParseResult	auth::JsonJwksParser::parse(
	const std::string &jsonBody
)
{
	try
	{
		const auto document = json::parse(jsonBody);
		const auto &keys = document.at("keys");
		if (keys.empty() || !keys.is_array())
			return {
				JwksParseResult::Status::InvalidJwks,
				{}
			};
		std::vector<auth::Jwk> result;
		for (const auto &key : keys)
		{
			const auto kid = key.at("kid").get<std::string>();
			const auto kty = key.at("kty").get<std::string>();
			const auto alg = key.at("alg").get<std::string>();
			const auto n = key.at("n").get<std::string>();
			const auto e = key.at("e").get<std::string>();
			if (kid.empty() ||
				kty != "RSA" ||
				alg.empty() ||
				n.empty() ||
				e.empty() ||
				!isValidBase64Url(n) ||
				!isValidBase64Url(e))
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