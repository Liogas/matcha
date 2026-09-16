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
			// testConnection(); // A RETIRER PLUS TARD
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
}

QueryResult	Database::executeParams(
	const std::string &query,
	const std::vector<std::string>& params
)
{
	if (!this->_connection)
	{
		std::cerr << "[ERROR BDD] No connection with BDD" << std::endl;
		return QueryResult();
	}
	std::vector<const char *> values;
	values.reserve(params.size());
	for (const std::string &param : params)
		values.push_back(param.c_str());
	std::cout 	<< "[INFO BDD] parameterized query sended : "
				<< query << std::endl;
	PGresult *result = PQexecParams(
		this->_connection,
		query.c_str(),
		static_cast<int>(params.size()),
		nullptr,
		values.data(),
		nullptr,
		nullptr,
		0
	);
	return QueryResult(result);
}

void	Database::testConnection()
{
	{std::cout << "TEST 1" << std::endl;
	QueryResult result = this->execute(
        "SELECT "
        "42 AS id, "
        "'Gaston' AS username, "
        "'Learning C++' AS bio"
    );

    if (result.isError())
    {
        std::cerr << "[ERROR BDD] Query failed: "
                  << PQresultErrorMessage(result.get())
                  << std::endl;
        return;
    }

    std::cout << "[SUCCESS BDD] Query successful !" << std::endl;

    std::cout << "Rows: "
              << result.rowCount()
              << std::endl;

    std::cout << "Columns: "
              << result.columnCount()
              << std::endl;

	// AFFICHER NOM DES COLONNES
	std::cout << std::endl;
	std::cout << "Test 2 : Nom colonnes" << std::endl;
	for (int column = 0; column < result.columnCount(); ++column)
	{
		std::cout << "Column " << column
				<< ": "
				<< result.columnName(column)
				<< std::endl;
	}

	// PARCOURT LA TABLE POUR TESTER QUERYRESULT.VALUE
	for (int row = 0; row < result.rowCount(); ++row)
	{
		for (int column = 0; column < result.columnCount(); ++column)
		{
			std::cout << result.columnName(column)
					<< " = "
					<< result.value(row, column)
					<< std::endl;
		}
	}

	// TEST GETSTRING ET GETINT
	std::cout << std::endl;
	std::cout << "Test 3 : getString et getInt" << std::endl;
	auto id = result.getInt(0, 0);
	auto username = result.getString(0, 1);
	auto bio3 = result.getString(0, 2);

	if (id)
		std::cout << "ID: " << *id << std::endl;

	if (username)
		std::cout << "Username: " << *username << std::endl;

	if (bio3)
		std::cout << "Bio: " << *bio3 << std::endl;
	

	// TEST VALEUR NULL
	std::cout << std::endl;
	std::cout << "Test 4 : valeur null" << std::endl;
	QueryResult nullResult = this->execute(
		"SELECT "
		"42 AS id, "
		"NULL AS bio"
	);

	std::cout 	<< "bio is NULL: "
				<< nullResult.isNull(0, 1)
				<< std::endl;
	auto bio2 = nullResult.getString(0, 1);
	if (!bio2)
    	std::cout << "bio has no value" << std::endl;

	// TEST POSITION INVALIDE
	std::cout << std::endl;
	std::cout << "Test 5 : position invalide" << std::endl;
	if (result.value(100, 0) == nullptr)
		std::cout << "Invalid row handled correctly" << std::endl;

	if (result.value(0, 100) == nullptr)
		std::cout << "Invalid column handled correctly" << std::endl;
	}
	// TEST ERREUR
	{std::cout << "Test 6 : erreur dans la requete" << std::endl;
	QueryResult errorResult = this->execute(
		"SELECT * FROM table_that_does_not_exist"
	);
	if (errorResult.isError())
	{
		std::cout << "SQL error handled correctly"
				<< std::endl;

		std::cout << "Erreur : " << PQresultErrorMessage(errorResult.get())
				<< std::endl;
	}}

	{std::cout << std::endl;
	std::cout << "Test 7 : getBool" << std::endl;
	QueryResult boolResult = this->execute(
		"SELECT "
		"true AS active, "
		"false AS banned, "
		"NULL AS deleted"
	);
	auto active = boolResult.getBool(0, 0);
	auto banned = boolResult.getBool(0, 1);
	auto deleted = boolResult.getBool(0, 2);
	if (active)
		std::cout << "active: " << *active << std::endl;
	if (banned)
		std::cout << "banned: " << *banned << std::endl;
	if (!deleted)
		std::cout << "deleted: NULL" << std::endl;}
	
	{
		std::cout << std::endl;
		std::cout << "Test 8 : test avec verif du type" << std::endl;
		QueryResult typeResult = this->execute(
			"SELECT "
			"42 AS id, "
			"'Gaston' AS username, "
			"true AS active"
		);
		auto id = typeResult.getInt(0, 0);
		auto username = typeResult.getString(0, 1);
		auto active = typeResult.getBool(0, 2);
		auto wrongInt = typeResult.getInt(0, 1);
		auto wrongBool = typeResult.getBool(0, 0);
		auto wrongString = typeResult.getString(0, 0);
		std::cout << "id = ";
		if (id.has_value())
			std::cout << *id << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
		std::cout << "username = ";
		if (username.has_value())
			std::cout << *username << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
		std::cout << "active = ";
		if (active.has_value())
			std::cout << *active << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
		std::cout << "wrongInt = ";
		if (wrongInt.has_value())
			std::cout << *wrongInt << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
		std::cout << "wrongBool = ";
		if (wrongBool.has_value())
			std::cout << *wrongBool << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
		std::cout << "wrongString = ";
		if (wrongString.has_value())
			std::cout << *wrongString << std::endl;
		else
			std::cout << "NULL/Incorrect type" << std::endl;
	}
	{
		QueryResult dateResult = execute(
			"SELECT DATE '1998-02-30' AS birth_date"
		);

		if (dateResult.isError())
		{
			std::cout << "[TEST DATE] Query error" << std::endl;
		}
		else
		{
			auto date = dateResult.getDate(0, 0);

			if (date)
			{
				std::cout << "[TEST DATE] Date converted correctly: "
						<< static_cast<int>(date->year()) << "-"
						<< static_cast<unsigned>(date->month()) << "-"
						<< static_cast<unsigned>(date->day())
						<< std::endl;
			}
			else
			{
				std::cout << "[TEST DATE] Date conversion failed"
						<< std::endl;
			}
		}
	}
	{
	std::cout << "Test Timestamp valide" << std::endl;
	QueryResult timestampResult = execute(
		"SELECT TIMESTAMP '2026-09-15 14:30:45' AS created_at"
	);

	auto timestamp = timestampResult.getTimestamp(0, 0);

	if (timestamp)
	{
		auto time = *timestamp;

		auto days = std::chrono::floor<std::chrono::days>(time);
		auto timeOfDay = time - days;

		auto date = std::chrono::year_month_day{days};

		auto hours = std::chrono::duration_cast<std::chrono::hours>(
			timeOfDay
		);

		timeOfDay -= hours;

		auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
			timeOfDay
		);

		timeOfDay -= minutes;

		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
			timeOfDay
		);

		if (static_cast<int>(date.year()) == 2026
			&& static_cast<unsigned>(date.month()) == 9
			&& static_cast<unsigned>(date.day()) == 15
			&& hours.count() == 14
			&& minutes.count() == 30
			&& seconds.count() == 45)
		{
			std::cout << "[TEST TIMESTAMP] Valid timestamp: OK"
					<< std::endl;
		}
		else
		{
			std::cout << "[TEST TIMESTAMP] Valid timestamp: FAILED"
					<< std::endl;
		}
	}
	else
	{
		std::cout << "[TEST TIMESTAMP] Conversion failed"
				<< std::endl;
	}
	}
	{
		std::cout << "Test 2 timestamp NULL" << std::endl;
		QueryResult nullTimestampResult = execute(
			"SELECT NULL::TIMESTAMP AS created_at"
		);

		auto nullTimestamp = nullTimestampResult.getTimestamp(0, 0);

		if (!nullTimestamp)
		{
			std::cout << "[TEST TIMESTAMP] NULL timestamp: OK"
					<< std::endl;
		}
		else
		{
			std::cout << "[TEST TIMESTAMP] NULL timestamp: FAILED"
					<< std::endl;
		}
	}
	{
		std::cout << "test timestamp 3 mauvais type" << std::endl;
		QueryResult wrongTypeTimestampResult = execute(
			"SELECT '2026-09-15 14:30:45' AS created_at"
		);

		auto wrongTypeTimestamp =
			wrongTypeTimestampResult.getTimestamp(0, 0);

		if (!wrongTypeTimestamp)
		{
			std::cout << "[TEST TIMESTAMP] Wrong type rejected: OK"
					<< std::endl;
		}
		else
		{
			std::cout << "[TEST TIMESTAMP] Wrong type rejected: FAILED"
					<< std::endl;
		}
	}
}