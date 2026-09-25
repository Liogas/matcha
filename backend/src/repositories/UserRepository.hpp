#pragma once

#include <auth/credentials/IUserRepository.hpp>
#include <string>

class Database;

class UserRepository : public auth::IUserRepository
{
	public:
		explicit UserRepository(Database &database);

		auth::UserLookupResult
			findByEmail(const std::string &email) override;
		auth::UserLookupResult
			findById(const std::int64_t id) override;
		auth::CreateUserResult createUser(
			const std::string &email,
			const std::string &pwdHash
		) override;
	private:
		Database &_database;
};