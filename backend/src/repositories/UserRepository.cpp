#include "UserRepository.hpp"
#include "../database/Database.hpp"

#include <iostream>

UserRepository::UserRepository(Database &database):
	_database(database)
{
}

bool	UserRepository::create(const std::string &email, const std::string &passwordHash)
{
	std::cout << "[WIP]" << std::endl;
	return (true);
}