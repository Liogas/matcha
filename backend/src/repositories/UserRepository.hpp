#pragma once

#include <auth/IUserRepository.hpp>
#include <string>

class Database;

class UserRepository : public auth::IUserRepository
{
	public:
		explicit UserRepository(Database &database);

		std::optional<auth::User>
			findByEmail(const std::string &email) override;
		std::optional<auth::User>
			findById(const std::int64_t id) override;
		std::optional<std::int64_t> createUser(
			const std::string &email,
			const std::string &pwdHash
		) override;
	private:
		Database &_database;
};