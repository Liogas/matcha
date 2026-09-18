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
	QueryResult result = this->_database.executeParams(
		"SELECT id, email, password_hash "
		"FROM users "
		"WHERE email = $1",
		{email}
	);
	if (result.isError())
	{
		std::cerr << "[ERROR UserRepository] SELECT failed" << std::endl;
		return std::nullopt;
	}

	if (result.rowCount() == 0)
	{
		std::cerr << "[ERROR UserRepository] SELECT return empty result" << std::endl;
		return std::nullopt;
	}
	std::cout 	<< "[DEBUG] rowCount = "
				<< result.rowCount() << std::endl;

	const auto id = result.getInt64(0,0);
	const auto userEmail = result.getString(0, 1);
	const auto passwordHash = result.getString(0, 2);

	std::cout	<< "[DEBUG] id = "
				<< (id.has_value() ? std::to_string(*id) : "null")
				<< std::endl;

	std::cout << "[DEBUG] email = "
              << (userEmail.has_value() ? *userEmail : "null")
              << std::endl;

	std::cout << "[DEBUG] passwordHash = "
              << (passwordHash.has_value() ? *passwordHash : "null")
              << std::endl;

	if (!id.has_value() || !userEmail.has_value() || !passwordHash.has_value())
		return std::nullopt;
	auth::User user{
		*id,
		*userEmail,
		*passwordHash
	};
	return user;
}

std::optional<auth::User>	UserRepository::findById(
	const std::int64_t id
)
{
	QueryResult result = this->_database.executeParams(
		"SELECT id, email, password_hash "
		"FROM users "
		"WHERE id = $1",
		{std::to_string(id)}
	);
	if (result.isError() || result.rowCount() == 0)
		return std::nullopt;
	const auto userId = result.getInt64(0, 0);
	const auto userEmail = result.getString(0, 1);
	const auto userPassword = result.getString(0, 2);
	if (!userId.has_value() || !userEmail.has_value() || !userPassword.has_value())
		return std::nullopt;
	return auth::User{
		*userId,
		*userEmail,
		*userPassword
	};
}

std::optional<std::int64_t>	UserRepository::createUser(
	const std::string &email,
	const std::string &pwdHash
)
{
	QueryResult result = this->_database.executeParams(
		"INSERT INTO users (email, password_hash) "
		"VALUES ($1, $2) "
		"RETURNING id",
		{email, pwdHash}
	);
	if (result.isError() || result.rowCount() == 0)
	{
		std::cerr << "[ERROR UserRepository] INSERT failed" << std::endl;
		return std::nullopt;
	}
	const auto id = result.getInt64(0, 0);
	if (!id.has_value())
		return std::nullopt;
	return *id;
}