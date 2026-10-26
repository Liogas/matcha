#include "Database.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <chrono>
#include <stdexcept>

namespace
{
	std::string readPasswordFile(const char *path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("Unable to read DB_PASSWORD_FILE");

		std::string password{
			std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>()
		};
		while (!password.empty() &&
			(password.back() == '\n' || password.back() == '\r'))
			password.pop_back();
		if (password.empty())
			throw std::runtime_error("DB_PASSWORD_FILE is empty");
		return password;
	}
}

Database::Database():
	_connection(nullptr)
{
	const char *host = std::getenv("DB_HOST");
	const char *port = std::getenv("DB_PORT");
	const char *dbname = std::getenv("DB_NAME");
	const char *user = std::getenv("DB_USER");
	const char *pwdFile = std::getenv("DB_PASSWORD_FILE");
	if (!host || !port || !dbname || !user || !pwdFile)
		throw std::runtime_error("Database environment is incomplete");
	const std::string pwd = readPasswordFile(pwdFile);

	std::string info =
		"host=" + std::string(host) +
		" port=" + std::string(port) +
		" dbname=" + std::string(dbname) +
		" user=" + std::string(user) +
		" password=" + pwd;

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
	std::lock_guard<std::mutex> lock(this->_connectionMutex);
	if (!this->_connection)
	{
		std::cerr << "[ERROR BDD] No connection with BDD" << std::endl;
		return QueryResult();
	}
	return QueryResult(PQexec(this->_connection, query.c_str()));
}

QueryResult	Database::executeParams(
	const std::string &query,
	const std::vector<std::string>& params
)
{
	std::lock_guard<std::mutex> lock(this->_connectionMutex);
	if (!this->_connection)
	{
		std::cerr << "[ERROR BDD] No connection with BDD" << std::endl;
		return QueryResult();
	}
	std::vector<const char *> values;
	values.reserve(params.size());
	for (const std::string &param : params)
		values.push_back(param.c_str());
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
