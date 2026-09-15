#pragma once

#include <string>

class Database;

class UserRepository
{
	public:
		explicit UserRepository(Database &database);
		bool	create(const std::string &email, const std::string &passwordHash);
	private:
		Database &_database;	
};