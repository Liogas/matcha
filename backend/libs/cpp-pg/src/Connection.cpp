#include <pg/Connection.hpp>
#include <pg/SqlState.hpp>
#include <libpq-fe.h>
#include <string>
#include <vector>

namespace
{
	// Les messages de libpq finissent par un retour à la ligne.
	std::string trimmed(const char *message)
	{
		std::string text = message ? message : "";
		while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
			text.pop_back();
		return text;
	}
}

void pg::Connection::Close::operator()(pg_conn *connection) const noexcept
{
	PQfinish(connection);
}

pg::Connection::Connection(pg_conn *connection)
	: _connection(connection)
{
}

core::Result<pg::Connection> pg::Connection::open(const ConnectionSettings &settings)
{
	const std::string timeout = std::to_string(settings.connectTimeoutSeconds);
	const char *const keys[] = {
		"host", "port", "dbname", "user", "password",
		"application_name", "connect_timeout", "client_encoding", "options",
		nullptr};
	const char *const values[] = {
		settings.host.c_str(), settings.port.c_str(), settings.database.c_str(),
		settings.user.c_str(), settings.password.c_str(),
		settings.applicationName.c_str(), timeout.c_str(), "UTF8",
		// Chaque session parle en UTC et en ISO : Row::get<Timestamp> sait lire la
		// réponse. Et pas de NOTICE, que libpq écrirait sur stderr.
		"-c TimeZone=UTC -c DateStyle=ISO -c client_min_messages=warning",
		nullptr};
	// 0 : dbname est un nom de base, jamais une chaîne de connexion à interpréter.
	Connection connection(PQconnectdbParams(keys, values, 0));
	if (connection.isHealthy())
		return connection;
	return core::fail(core::ErrorCode::Unavailable,
					  "cannot connect to PostgreSQL at " + settings.host + ":" + settings.port + ": " + trimmed(PQerrorMessage(connection._connection.get())));
}

core::Result<pg::Rows> pg::Connection::execute(std::string_view sql, const Params &params)
{
	const std::string text(sql); // libpq veut une chaîne terminée par '\0'
	std::vector<const char *> values;
	values.reserve(params.size());
	for (const Text &value : params.values())
		values.push_back(value ? value->c_str() : nullptr); // nullptr = NULL

	pg_result *raw = PQexecParams(this->_connection.get(), text.c_str(),
								  static_cast<int>(values.size()),
								  nullptr, // types des paramètres : déduits par le serveur
								  values.data(),
								  nullptr, // longueurs : inutiles en format texte
								  nullptr, // formats des paramètres : tous en texte
								  0);	   // format du résultat : texte
	if (raw == nullptr)
		return core::fail(core::ErrorCode::Unavailable,
						  trimmed(PQerrorMessage(this->_connection.get())));
	Rows rows(raw); // possède le résultat dès maintenant, même en cas d'erreur

	const ExecStatusType status = PQresultStatus(raw);
	if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
		return rows;
	const char *state = PQresultErrorField(raw, PG_DIAG_SQLSTATE);
	if (state == nullptr) // erreur produite par libpq elle-même : connexion perdue
		return core::fail(core::ErrorCode::Unavailable, trimmed(PQresultErrorMessage(raw)));
	return core::fail(errorCodeFor(state),
					  trimmed(PQresultErrorField(raw, PG_DIAG_MESSAGE_PRIMARY)) + " (SQLSTATE " + state + ")");
}

bool pg::Connection::isHealthy() const
{
	return PQstatus(this->_connection.get()) == CONNECTION_OK;
}

bool pg::Connection::inTransaction() const
{
	const PGTransactionStatusType status = PQtransactionStatus(this->_connection.get());
	return status == PQTRANS_INTRANS || status == PQTRANS_INERROR;
}

bool pg::Connection::reset()
{
	PQreset(this->_connection.get());
	return this->isHealthy();
}