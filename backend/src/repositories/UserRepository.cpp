#include "UserRepository.hpp"
#include "../database/Database.hpp"

#include <iostream>

UserRepository::UserRepository(Database &database):
	_database(database)
{
}

auth::UserLookupResult	UserRepository::findByEmail(
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
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	}

	if (result.rowCount() == 0)
	{
		std::cerr << "[ERROR UserRepository] SELECT return empty result" << std::endl;
		return {
			auth::UserLookupResult::Status::NotFound,
			std::nullopt
		};
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
	{
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	}
	return {
		auth::UserLookupResult::Status::Found,
		auth::User {
			*id,
			*userEmail,
			*passwordHash
		}
	};
}

auth::UserLookupResult	UserRepository::findById(
	const std::int64_t id
)
{
	QueryResult result = this->_database.executeParams(
		"SELECT id, email, password_hash "
		"FROM users "
		"WHERE id = $1",
		{std::to_string(id)}
	);
	if (result.isError())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	if (result.rowCount() == 0)
		return {
			auth::UserLookupResult::Status::NotFound,
			std::nullopt
		};
	const auto userId = result.getInt64(0, 0);
	const auto userEmail = result.getString(0, 1);
	const auto userPassword = result.getString(0, 2);
	if (!userId.has_value() || !userEmail.has_value() || !userPassword.has_value())
		return {
			auth::UserLookupResult::Status::DatabaseError,
			std::nullopt
		};
	return {
		auth::UserLookupResult::Status::Found,
		auth::User{
		   *userId,
		   *userEmail,
		   *userPassword
	   }
	};
}

auth::CreateUserResult	UserRepository::createUser(
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
	if (result.isError())
	{
		if (result.sqlState() == "23505")
			return {
				auth::CreateUserResult::Status::EmailAlreadyExists,
				std::nullopt
			};
		return {
			auth::CreateUserResult::Status::DatabaseError,
			std::nullopt
		};
	}
	if (result.rowCount() == 0)
		return {
			auth::CreateUserResult::Status::DatabaseError,
			std::nullopt
		};
	const auto id = result.getInt64(0, 0);
	if (!id.has_value())
		return {
			auth::CreateUserResult::Status::DatabaseError,
			std::nullopt
		};
	return {
		auth::CreateUserResult::Status::Success,
		*id
	};
}