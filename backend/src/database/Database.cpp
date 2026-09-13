#include "Database.hpp"

#include <cstdlib>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

Database::Database():
	_connection(nullptr)
{
	const char *host = std::getenv("DB_HOST");
	const char *port = std::getenv("DB_PORT");
	const char *dbname = std::getenv("DB_NAME");
	const char *user = std::getenv("DB_USER");
	const char *pwd = std::getenv("DB_PASSWORD");

	std::string info =
		"host=" + std::string(host) +
		" port=" + std::string(port) +
		" dbname=" + std::string(dbname) +
		" user=" + std::string(user) +
		" password=" + std::string(pwd);

	constexpr int max = 12;
	constexpr int delay = 5;

	for (int i = 1; i < max; ++i)
	{
		std::cout 	<< "[INFO BDD] Connecting to database (attempt "
					<< i << "/ " << max << ")" << std::endl;
		this->_connection = PQconnectdb(info.c_str());
		if (PQstatus(this->_connection) == CONNECTION_OK)
		{
			std::cout << "[SUCCESS BDD] Database connection ok" << std::endl;
			testConnection(); // A RETIRER PLUS TARD
			return ;
		}
		std::cerr 	<< "[ERROR BDD] Database connection failed: "
					<< PQerrorMessage(this->_connection) << std::endl;
		PQfinish(this->_connection);
		this->_connection = nullptr;

		if (i < max)
		{
			std::cout	<< "[INFO BDD] Retrying in "
						<< delay << " seconds" << std::endl;
			std::this_thread::sleep_for(
				std::chrono::seconds(delay)
			);
		}
	}
	std::cerr 	<< "[ERROR BDD] Could not connect to database after "
				<< max << " attempts" << std::endl;
}

Database::~Database()
{
	if (this->_connection)
		PQfinish(this->_connection);
}

void	Database::testConnection()
{
	PGresult *result = PQexec(this->_connection, "SELECT 1");

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		std::cerr	<< "[ERROR BDD] Query failed: "
					<< PQerrorMessage(this->_connection)
					<< std::endl;
		PQclear(result);
		return ;
	}
	std::cout << "[SUCCESS BDD] Query successful !" << std::endl;
	std::cout 	<< "Result: "
				<< PQgetvalue(result, 0, 0) << std::endl;
	PQclear(result);
}