#include "UserRepository.hpp"
#include "../database/Database.hpp"

#include <iostream>

UserRepository::UserRepository(Database &database):
	_database(database)
{
}

std::optional<auth::User>	UserRepository::findByEmail(
	const std::string &email
)
{
	return std::nullopt;
}

std::optional<auth::User>	UserRepository::findById(
	const std::int64_t id
)
{
	return std::nullopt;
}

std::optional<std::int64_t>	UserRepository::createUser(
	const std::string &email,
	const std::string &pwdHash
)
{
	return std::nullopt;
}