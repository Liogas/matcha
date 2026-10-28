#include <pg/Pool.hpp>
#include <cassert>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
	std::string_view beginStatement(pg::Isolation isolation)
	{
		switch (isolation)
		{
			case pg::Isolation::ReadCommitted:
				return "BEGIN ISOLATION LEVEL READ COMMITTED";
			case pg::Isolation::RepeatableRead:
				return "BEGIN ISOLATION LEVEL REPEATABLE READ";
			case pg::Isolation::Serializable:
				return "BEGIN ISOLATION LEVEL SERIALIZABLE";
		}
		return "BEGIN";
	}
}

// ---- PooledConnection ----

pg::PooledConnection::PooledConnection(ConnectionPool &pool, Connection connection)
	: _pool(&pool), _connection(std::move(connection))
{
}

pg::PooledConnection::PooledConnection(PooledConnection &&other) noexcept
	: _pool(std::exchange(other._pool, nullptr)), _connection(std::move(other._connection))
{
}

pg::PooledConnection &pg::PooledConnection::operator=(PooledConnection &&other) noexcept
{
	if (this != &other)
	{
		this->giveBack();
		this->_pool = std::exchange(other._pool, nullptr);
		this->_connection = std::move(other._connection);
	}
	return *this;
}

pg::PooledConnection::~PooledConnection()
{
	this->giveBack();
}

void pg::PooledConnection::giveBack() noexcept
{
	if (this->_pool != nullptr)
		std::exchange(this->_pool, nullptr)->release(std::move(this->_connection));
}

pg::Connection &pg::PooledConnection::operator*()
{
	return this->_connection;
}

pg::Connection *pg::PooledConnection::operator->()
{
	return &this->_connection;
}

// ---- Transaction ----

pg::Transaction::Transaction(PooledConnection connection)
	: _connection(std::move(connection)), _open(true)
{
}

pg::Transaction::Transaction(Transaction &&other) noexcept
	: _connection(std::move(other._connection)), _open(std::exchange(other._open, false))
{
}

pg::Transaction::~Transaction()
{
	if (this->_open)
		(void)this->_connection->execute("ROLLBACK");
}

void pg::Transaction::requireOpen() const
{
	if (!this->_open)
		throw std::logic_error("pg::Transaction used after commit() or rollback()");
}

core::Result<pg::Rows> pg::Transaction::execute(std::string_view sql, const Params &params)
{
	this->requireOpen();
	return this->_connection->execute(sql, params);
}

core::Result<void> pg::Transaction::commit()
{
	this->requireOpen();
	this->_open = false;
	const auto result = this->_connection->execute("COMMIT");
	if (!result)
		return std::unexpected(result.error());
	// Après une erreur dans la transaction, PostgreSQL répond à COMMIT par
	// « ROLLBACK », sans erreur : rien n'a été enregistré.
	if (result->command() == "ROLLBACK")
		return core::fail(core::ErrorCode::Internal,
						  "transaction rolled back: an earlier query in it failed");
	return {};
}

core::Result<void> pg::Transaction::rollback()
{
	this->requireOpen();
	this->_open = false;
	const auto result = this->_connection->execute("ROLLBACK");
	if (!result)
		return std::unexpected(result.error());
	return {};
}

// ---- ConnectionPool ----

core::Result<std::unique_ptr<pg::ConnectionPool>> pg::ConnectionPool::open(
	const ConnectionSettings &settings, const PoolOptions &options, core::ILogger &logger)
{
	if (options.size == 0)
		return core::fail(core::ErrorCode::Invalid, "pool size must be at least 1");
	std::vector<Connection> connections;
	connections.reserve(options.size);

	// La première connexion attend que le serveur soit prêt.
	for (int attempt = 1;; ++attempt)
	{
		auto connection = Connection::open(settings);
		if (connection)
		{
			connections.push_back(std::move(*connection));
			break;
		}
		logger.warning("PostgreSQL connection failed", {{"attempt", std::to_string(attempt) + "/" + std::to_string(options.connectAttempts)},
														{"error", connection.error().message}});
		if (attempt >= options.connectAttempts)
			return std::unexpected(connection.error());
		std::this_thread::sleep_for(options.retryDelay);
	}
	// Les suivantes : le serveur répond, un seul essai chacune.
	while (connections.size() < options.size)
	{
		auto connection = Connection::open(settings);
		if (!connection)
			return std::unexpected(connection.error());
		connections.push_back(std::move(*connection));
	}
	logger.info("PostgreSQL pool ready", {{"host", settings.host},
										  {"database", settings.database},
										  {"size", std::to_string(options.size)}});
	return std::unique_ptr<ConnectionPool>(
		new ConnectionPool(options, logger, std::move(connections)));
}

pg::ConnectionPool::ConnectionPool(const PoolOptions &options, core::ILogger &logger,
								   std::vector<Connection> connections)
	: _options(options), _logger(logger), _idle(std::move(connections))
{
}

pg::ConnectionPool::~ConnectionPool()
{
	// Une connexion encore prêtée garderait un pointeur vers ce pool détruit.
	assert(this->_idle.size() == this->_options.size && "pg::ConnectionPool destroyed while connections are still borrowed");
}

core::Result<pg::PooledConnection> pg::ConnectionPool::acquire()
{
	return this->acquire(this->_options.acquireTimeout);
}

core::Result<pg::PooledConnection> pg::ConnectionPool::acquire(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(this->_mutex);
	if (!this->_available.wait_for(lock, timeout, [this]
								   { return !this->_idle.empty(); }))
		return core::fail(core::ErrorCode::Unavailable,
						  "no PostgreSQL connection available after " + std::to_string(timeout.count()) + " ms");
	Connection connection = std::move(this->_idle.back());
	this->_idle.pop_back();
	return PooledConnection(*this, std::move(connection));
}

core::Result<pg::Transaction> pg::ConnectionPool::begin(Isolation isolation)
{
	auto connection = this->acquire();
	if (!connection)
		return std::unexpected(connection.error());
	const auto started = (*connection)->execute(beginStatement(isolation));
	if (!started)
		return std::unexpected(started.error());
	return Transaction(std::move(*connection));
}

std::size_t pg::ConnectionPool::idleCount() const
{
	const std::lock_guard lock(this->_mutex);
	return this->_idle.size();
}

void pg::ConnectionPool::release(Connection connection) noexcept
{
	// Hors du verrou : ROLLBACK et PQreset parlent au serveur, ça peut prendre du temps.
	if (connection.inTransaction())
		(void)connection.execute("ROLLBACK");
	if (!connection.isHealthy())
	{
		this->_logger.warning("PostgreSQL connection lost, reconnecting");
		if (!connection.reset())
			this->_logger.error("PostgreSQL reconnection failed");
	}
	{
		const std::lock_guard lock(this->_mutex);
		// Ne réalloue jamais : la capacité vaut déjà la taille du pool.
		this->_idle.push_back(std::move(connection));
	}
	this->_available.notify_one();
}