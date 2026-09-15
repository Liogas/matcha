#include "UserRepository.hpp"
#include "../database/Database.hpp"

UserRepository::UserRepository(Database &database):
	_database(database)
{
}

bool	UserRepository::create(const std::string &email, const std::string &passwordHash)
{
	
}