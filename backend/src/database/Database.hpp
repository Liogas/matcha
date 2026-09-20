#pragma once

#include <string>
#include <vector>

#include "QueryResult.hpp"

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
		PGconn*	_connection;
};