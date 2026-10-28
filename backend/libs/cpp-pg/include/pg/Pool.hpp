#pragma once
#include <core/Logger.hpp>
#include <core/Result.hpp>
#include <pg/Connection.hpp>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace pg
{
	class ConnectionPool;

	// Une connexion empruntée au pool, rendue automatiquement à sa destruction.
	class PooledConnection
	{
	public:
		PooledConnection(PooledConnection &&other) noexcept;
		PooledConnection &operator=(PooledConnection &&other) noexcept;
		PooledConnection(const PooledConnection &) = delete;
		PooledConnection &operator=(const PooledConnection &) = delete;
		~PooledConnection();

		Connection &operator*();
		Connection *operator->();

	private:
		friend class ConnectionPool;
		PooledConnection(ConnectionPool &pool, Connection connection);
		void giveBack() noexcept;

		ConnectionPool *_pool; // nullptr : rien à rendre (objet déplacé)
		Connection _connection;
	};

	enum class Isolation
	{
		ReadCommitted,
		RepeatableRead,
		Serializable
	};

	// BEGIN à la création, ROLLBACK à la destruction si commit() n'a pas été
	// appelé. Toutes les requêtes passent par la même connexion, du BEGIN au
	// COMMIT : personne d'autre ne peut s'y glisser.
	class Transaction
	{
	public:
		Transaction(Transaction &&other) noexcept;
		Transaction &operator=(Transaction &&) = delete;
		~Transaction();

		core::Result<Rows> execute(std::string_view sql, const Params &params = {});

		template <class... Args>
		core::Result<Rows> query(std::string_view sql, const Args &...args)
		{
			return this->execute(sql, pg::params(args...));
		}

		core::Result<void> commit();
		core::Result<void> rollback();

	private:
		friend class ConnectionPool;
		explicit Transaction(PooledConnection connection);
		void requireOpen() const;

		PooledConnection _connection;
		bool _open;
	};

	struct PoolOptions
	{
		std::size_t size = 4;
		std::chrono::milliseconds acquireTimeout{2000};
		int connectAttempts = 12; // pour la 1re connexion
		std::chrono::milliseconds retryDelay{5000};
	};

	// N connexions ouvertes au démarrage, prêtées une par une. Thread-safe.
	// Doit vivre plus longtemps que toutes les connexions et transactions prêtées.
	class ConnectionPool
	{
	public:
		static core::Result<std::unique_ptr<ConnectionPool>> open(
			const ConnectionSettings &settings,
			const PoolOptions &options,
			core::ILogger &logger);

		ConnectionPool(const ConnectionPool &) = delete;
		ConnectionPool &operator=(const ConnectionPool &) = delete;
		~ConnectionPool();

		core::Result<PooledConnection> acquire();
		core::Result<PooledConnection> acquire(std::chrono::milliseconds timeout);
		core::Result<Transaction> begin(Isolation isolation = Isolation::ReadCommitted);

		// Une requête seule, hors transaction : emprunte une connexion le temps de la requête.
		template <class... Args>
		core::Result<Rows> query(std::string_view sql, const Args &...args)
		{
			auto connection = this->acquire();
			if (!connection)
				return std::unexpected(connection.error());
			return (*connection)->query(sql, args...);
		}

		std::size_t idleCount() const;

	private:
		friend class PooledConnection;
		ConnectionPool(const PoolOptions &options, core::ILogger &logger,
					   std::vector<Connection> connections);
		void release(Connection connection) noexcept;

		PoolOptions _options;
		core::ILogger &_logger;
		mutable std::mutex _mutex;
		std::condition_variable _available;
		std::vector<Connection> _idle;
	};
}