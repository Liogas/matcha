#pragma once

#include <string>
#include "QueryResult.hpp"

class Database
{
	public:
		Database();
		~Database();
		QueryResult	execute(const std::string &query);
	private:
		PGconn*	_connection; // Représente la connexion a PostgreSQL

		void	testConnection();
};