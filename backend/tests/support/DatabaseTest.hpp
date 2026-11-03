#pragma once
#include <core/Logger.hpp>
#include <pg/Executor.hpp>
#include <pg/Pool.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <sstream>

// Base des tests qui parlent à la base de test (variables PG_TEST_*), avec le
// schéma de matcha appliqué par pg::migrate. Sans PG_TEST_HOST : sautés.
class DatabaseTest : public testing::Test
{
protected:
	void SetUp() override;
	virtual std::size_t poolSize() const { return 2; }

	// Détruits dans l'ordre inverse : le pool avant le logger qu'il utilise.
	std::ostringstream logs;
	core::StreamLogger logger{logs, core::LogLevel::Warning};
	std::unique_ptr<pg::ConnectionPool> pool;
};

// Chaque requête passe par un SAVEPOINT. Si elle échoue, ROLLBACK TO SAVEPOINT
// n'annule qu'elle : la transaction du test reste utilisable, comme si la
// requête avait été envoyée seule.
class SavepointExecutor : public pg::Executor
{
public:
	explicit SavepointExecutor(pg::Transaction &transaction);
	core::Result<pg::Rows> execute(std::string_view sql, const pg::Params &params = {}) override;

private:
	pg::Transaction &_transaction;
};

// Pour tester un repository : tout le test se passe dans une transaction
// annulée à la fin. Il ne laisse aucune trace, même s'il échoue en route.
class RepositoryTest : public DatabaseTest
{
protected:
	void SetUp() override;
	pg::Executor &database() { return *this->_statements; }

private:
	// Après pool : détruits avant lui. La transaction fait ROLLBACK, puis rend sa connexion.
	std::optional<pg::Transaction> _transaction;
	std::optional<SavepointExecutor> _statements;
};