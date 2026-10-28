#pragma once
#include <core/Result.hpp>
#include <pg/Executor.hpp>
#include <pg/Settings.hpp>
#include <memory>
#include <string_view>

// Le type de libpq (PGconn), déclaré sans inclure libpq-fe.h.
struct pg_conn;

namespace pg
{
	// Une connexion à PostgreSQL. Elle possède son PGconn : non copiable,
	// déplaçable, fermée (PQfinish) à sa destruction.
	class Connection : public Executor
	{
	public:
		static core::Result<Connection> open(const ConnectionSettings &settings);

		// Une seule instruction SQL. Les valeurs passent en $1, $2…,
		// jamais collées dans le texte de la requête.
		core::Result<Rows> execute(std::string_view sql, const Params &params = {}) override;
		
		// Un script de plusieurs instructions (un fichier de migration), sans
		// paramètres. À réserver à du SQL de confiance, jamais à une saisie.
		core::Result<void> executeScript(std::string_view sql);

		bool isHealthy() const;		// la connexion au serveur est-elle ouverte ?
		bool inTransaction() const; // un BEGIN sans COMMIT ni ROLLBACK ?
		bool reset();				// se reconnecter (PQreset) ; true si ça a marché

	private:
		struct Close
		{
			void operator()(pg_conn *connection) const noexcept;
		};

		explicit Connection(pg_conn *connection);

		std::unique_ptr<pg_conn, Close> _connection;
	};
}