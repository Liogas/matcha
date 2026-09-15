#pragma once

#include <string>
#include "QueryResult.hpp"
#include <vector>

class Database
{
	public:
		Database();
		~Database();
		QueryResult	execute(const std::string &query);
		QueryResult executeParams(
			const std::string &query,
			const std::vector<std::string> &params
		);
	private:
		PGconn*	_connection; // Représente la connexion a PostgreSQL
		void	testConnection();
};