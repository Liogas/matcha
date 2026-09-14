#include "Database.hpp"

#include <cstdlib>
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

QueryResult	Database::execute(const std::string &query)
{
	if (!this->_connection)
	{
		std::cerr << "[ERROR BDD] No connection with BDD" << std::endl;
		return QueryResult();
	}
	std::cout 	<< "[INFO BDD] Query sended : "
				<< query << std::endl;
	return QueryResult(PQexec(this->_connection, query.c_str()));
	// if (PQresultStatus(result) != PGRES_TUPLES_OK)
	// {
	// 	std::cerr	<< "[ERROR BDD] Query failed: "
	// 				<< PQerrorMessage(this->_connection)
	// 				<< std::endl;
	// 	PQclear(result);
	// 	return (nullptr);
	// }
	// return (res);
}

void	Database::testConnection()
{
	QueryResult result(this->execute(
		"SELECT 1 as id, "
		"'Gaston' AS username, "
		"25 AS age, "
		"NULL AS bio, "
		"true AS active"
	));

	if (result.isError())
	{
		std::cerr	<< "[ERROR BDD] Query failed: "
					<< PQerrorMessage(this->_connection)
					<< std::endl;
		return ;
	}
	
	std::cout << "[SUCCESS BDD] Query successful !" << std::endl;
	if (result.isTuples())
	{
		std::cout << "Query tuples" << std::endl;
		std::cout << "Result: " << std::endl;
		std::cout << "Rows: " << result.rowCount() << std::endl;
		std::cout << "Columns: " << result.columnCount();
		std::cout << std::endl << std::endl;
		for (int i = 0; i < result.rowCount(); ++i)
		{
			for (int j = 0; j < result.columnCount(); ++j)
			{
				std::cout 	<< result.columnName(j) << " | Type OID "
							<< result.columnType(j) << " = ";
				if (result.isNull(i, j))
					std::cout << "NULL";
				else
					std::cout << result.value(i, j);
				std::cout << std::endl;
			}
			std::cout << std::endl;
		}
		auto username = result.getString(0,1);
		auto bio = result.getString(0,3);
		if (username.has_value())
			std::cout << "Username: " << username.value() << std::endl;
		if (!bio.has_value())
			std::cout << "Bio is NULL" << std::endl;
	} else if (result.isCommand())
	{
		std::cout << "Query command WIP" << std::endl;
	}
}