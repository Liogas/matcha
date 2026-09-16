#pragma once
#include "Database.hpp"

class DatabaseInitializer
{
	public:
		explicit DatabaseInitializer(Database &database);
		bool run();
	private:
		Database	&_database;
		bool	createMigrationTable();
		bool	runMigrations();
};