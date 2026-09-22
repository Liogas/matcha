#include "PasswordHasher.hpp"
#include <sodium.h>
#include <stdexcept>

namespace auth
{
	PasswordHasher::PasswordHasher()
	{
		if (sodium_init() < 0)
			throw std::runtime_error(
				"libsodium initialization failed"
			);
	}
	std::string PasswordHasher::hash(const std::string &password)
	{
		char hash[crypto_pwhash_STRBYTES];
		if (crypto_pwhash_str(
			hash,
			password.c_str(),
			password.size(),
			crypto_pwhash_OPSLIMIT_INTERACTIVE,
			crypto_pwhash_MEMLIMIT_INTERACTIVE
		) != 0)
		{
			throw std::runtime_error(
				"password hashing failed"
			);
		}
		return std::string(hash);
	}
	bool	PasswordHasher::verify(
		const std::string &password,
		const std::string &passwordHash
	)
	{
		return crypto_pwhash_str_verify(
			passwordHash.c_str(),
			password.c_str(),
			password.size()
		) == 0;
	}
}