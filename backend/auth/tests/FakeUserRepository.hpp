#pragma once

#include <auth/IUserRepository.hpp>
#include <vector>

class FakeUserRepository : public auth::IUserRepository
{
	public:
		bool	failCreateUser = false;
		std::optional<auth::User>
			findByEmail(const std::string &email) override
			{
				for (const auto &user : this->_users)
				{
					if (user.email == email)
						return user;
				}
				return std::nullopt;
			}
		std::optional<auth::User>
			findById(std::int64_t id) override
			{
				for (const auto &user : this->_users)
				{
					if (user.id == id)
						return user;
				}
				return std::nullopt;
			}
		std::optional<std::int64_t>
			createUser(
				const std::string &email,
				const std::string &passwordHash
			) override
			{
				if (failCreateUser)
					return std::nullopt;
				const std::int64_t id = this->_nextId++;
				this->_users.push_back({
					id,
					email,
					passwordHash
				});
				return id;
			}
			const std::vector<auth::User> &users() const
			{
				return this->_users;
			}
	private:
		std::vector<auth::User> _users;
		std::int64_t			_nextId = 1;
};