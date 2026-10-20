#include "OpaqueToken.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <stdexcept>

namespace
{
	void initializeSodium()
	{
		static std::once_flag sodiumInitialization;
		static int initializationResult = -1;
		std::call_once(sodiumInitialization, []
		{
			initializationResult = sodium_init();
		});
		if (initializationResult < 0)
			throw std::runtime_error(
				"Unable to initialize secure random source"
			);
	}
}

std::string hashOpaqueToken(const std::string &token)
{
	if (token.size() != 64 ||
		!std::all_of(token.begin(), token.end(), [](unsigned char character)
		{
			return (character >= '0' && character <= '9') ||
				(character >= 'a' && character <= 'f');
		}))
		throw std::invalid_argument("Invalid opaque token format");

	initializeSodium();
	std::array<unsigned char, crypto_hash_sha256_BYTES> digest{};
	if (crypto_hash_sha256(
			digest.data(),
			reinterpret_cast<const unsigned char *>(token.data()),
			token.size()
		) != 0)
		throw std::runtime_error("Unable to hash opaque token");

	std::array<char, crypto_hash_sha256_BYTES * 2 + 1> hash{};
	sodium_bin2hex(
		hash.data(),
		hash.size(),
		digest.data(),
		digest.size()
	);
	return std::string(hash.data(), hash.size() - 1);
}

OpaqueToken generateOpaqueToken()
{
	initializeSodium();

	std::array<unsigned char, 32> randomBytes{};
	randombytes_buf(randomBytes.data(), randomBytes.size());

	std::array<char, 65> token{};
	sodium_bin2hex(
		token.data(),
		token.size(),
		randomBytes.data(),
		randomBytes.size()
	);
	std::string value(token.data(), 64);
	return {value, hashOpaqueToken(value)};
}
