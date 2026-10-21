#include "DatabaseInitializer.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <cstdint>

namespace
{
	constexpr std::int64_t MIGRATION_LOCK_KEY = 0x4D4154434841;

	class MigrationLock
	{
		public:
			explicit MigrationLock(Database &database):
				_database(database),
				_acquired(!_database.execute(
					"SELECT pg_advisory_lock(" +
					std::to_string(MIGRATION_LOCK_KEY) + ")").isError())
			{}

			~MigrationLock()
			{
				if (_acquired)
					_database.execute(
						"SELECT pg_advisory_unlock(" +
						std::to_string(MIGRATION_LOCK_KEY) + ")");
			}

			bool acquired() const { return _acquired; }

		private:
			Database &_database;
			bool _acquired;
	};
}

DatabaseInitializer::DatabaseInitializer(Database &database):
	_database(database)
{
}

bool DatabaseInitializer::run()
{
	std::cout << "[INFO BDD] Initializing database..." << std::endl;
	MigrationLock migrationLock(_database);
	if (!migrationLock.acquired())
	{
		std::cerr << "[ERROR BDD] Could not acquire migration lock"
			<< std::endl;
		return false;
	}
	if (!this->createMigrationTable())
		return (false);
	if (!this->runMigrations())
		return (false);
	std::cout << "[SUCCES BDD] Database initialization complete" << std::endl;
	return (true);
}

bool	DatabaseInitializer::createMigrationTable()
{
	const std::string query = R"(
		CREATE TABLE IF NOT EXISTS schema_migrations (
			version 	INTEGER PRIMARY KEY,
			applied_at	TIMESTAMPTZ NOT NULL DEFAULT NOW()
		);
	)";
	QueryResult result = this->_database.execute(query);
	if (result.isError())
	{
		std::cerr << "[ERROR BDD] could not create migration table" << std::endl;
		return (false);
	}
	return (true);
}

bool	DatabaseInitializer::runMigrations()
{
	namespace fs = std::filesystem;
	const fs::path migrationDirectory = "migrations";
	if (!fs::exists(migrationDirectory))
	{
		std::cerr << "[ERROR BDD] Migration directory does not exist" << std::endl;
		return (false);
	}
	std::set<fs::path> migrationFiles;
	for (const auto &entry : fs::directory_iterator(migrationDirectory))
	{
		if (!entry.is_regular_file())
			continue ;
		if (entry.path().extension() != ".sql")
			continue ;
		migrationFiles.insert(entry.path());
	}
	for (const auto &migration : migrationFiles)
	{
		const std::string filename = migration.filename().string();
		std::smatch match;
		if (!std::regex_match(filename, match,
			std::regex(R"(^([0-9]+)_.+\.sql$)")))
		{
			std::cerr	<< "[ERROR BDD] Invalid migration filename: "
						<< filename << std::endl;
			return (false);
		}
		const int version = std::stoi(match[1].str());
		std::cout 	<< "[INFO BDD] Found migration: "
					<< migration.filename() << std::endl;
		const std::string query = "SELECT version FROM schema_migrations WHERE version = "
			+ std::to_string(version) + ";";
		QueryResult result = this->_database.execute(query);
		if (result.isError())
		{
			std::cerr	<< "[ERROR BDD] Could not check migration version: "
						<< version << std::endl;
			return (false);
		}
		if (result.rowCount() > 0)
		{
			std::cout	<< "[INFO BDD] Migration already applied: "
						<< version << std::endl;
			continue ;
		}
		std::ifstream file(migration);
		if (!file.is_open())
		{
			std::cerr	<< "[ERROR BDD] Could not open migration: "
						<< migration << std::endl;
			return (false);
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		const std::string sql = buffer.str();
		std::cout	<< "[INFO BDD] Executing migration: "
					<< migration.filename() << std::endl;
		if (this->_database.execute("BEGIN").isError())
			return (false);
		QueryResult migrationResult = this->_database.execute(sql);
		if (migrationResult.isError())
		{
			std::cerr	<< "[ERROR BDD] Migration failed: "
						<< migration.filename() << std::endl;
			this->_database.execute("ROLLBACK");
			return (false);
		}
		const std::string insertQuery =
			"INSERT INTO schema_migrations (version) VALUES ("
			+ std::to_string(version) + ");";
		QueryResult insertResult = this->_database.execute(insertQuery);
		if (insertResult.isError())
		{
			std::cerr	<< "[ERROR BDD] Could not record migration: "
						<< version << std::endl;
			this->_database.execute("ROLLBACK");
			return (false);
		}
		if (this->_database.execute("COMMIT").isError())
		{
			std::cerr	<< "[ERROR BDD] Could not commit migration: "
						<< version << std::endl;
			this->_database.execute("ROLLBACK");
			return (false);
		}
	}
	return (true);
}
