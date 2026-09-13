#pragma once

#include <libpq-fe.h>

class Database
{
	public:
		Database();
		~Database();
	private:
		PGconn*	_connection; // Représente la connexion a PostgreSQL

		void	testConnection();
};