#pragma once

#include <auth/IUserRepository.hpp>
#include <vector>
#include <optional>

class FakeUserRepository : public auth::IUserRepository
{
	public:
		bool	failCreateUser	= false;
		bool	failFindUser	= false;
		std::optional<auth::CreateUserResult::Status>
			forcedStatus;
		auth::UserLookupResult
			findByEmail(const std::string &email) override
			{
				if (failFindUser)
					return {
						auth::UserLookupResult::Status::DatabaseError,
						std::nullopt
					};
				for (const auto &user : this->_users)
				{
					if (user.email == email)
						return {
							auth::UserLookupResult::Status::Found,
							user
						};
				}
				return {
					auth::UserLookupResult::Status::NotFound,
					std::nullopt
				};
			}
		auth::UserLookupResult
			findById(std::int64_t id) override
			{
				if (failFindUser)
					return {
						auth::UserLookupResult::Status::DatabaseError,
						std::nullopt
					};
				for (const auto &user : this->_users)
				{
					if (user.id == id)
						return {
							auth::UserLookupResult::Status::Found,
							user
						};
				}
				return {
					auth::UserLookupResult::Status::NotFound,
					std::nullopt
				};
			}
		auth::CreateUserResult
			createUser(
				const std::string &email,
				const std::string &passwordHash
			) override
			{
				if (this->forcedStatus.has_value())
					return {
						*(this->forcedStatus),
						std::nullopt
					};
				if (failCreateUser)
					return {
						auth::CreateUserResult::Status::DatabaseError,
						std::nullopt
					};
				for (const auto &user : this->_users)
				{
					if (user.email == email)
					{
						return {
							auth::CreateUserResult::Status::EmailAlreadyExists,
							std::nullopt
						};
					}
				}
				const std::int64_t id = this->_nextId++;
				this->_users.push_back({
					id,
					email,
					passwordHash
				});
				return {
					auth::CreateUserResult::Status::Success,
					id
				};
			}
			const std::vector<auth::User> &users() const
			{
				return this->_users;
			}
	private:
		std::vector<auth::User> _users;
		std::int64_t			_nextId = 1;
};