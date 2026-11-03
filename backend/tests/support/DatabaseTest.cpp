#include "DatabaseTest.hpp"

#include <core/ConfigReader.hpp>
#include <pg/Migrations.hpp>

void DatabaseTest::SetUp()
{
	const core::SystemEnvironment environment;
	if (!environment.get("PG_TEST_HOST"))
		GTEST_SKIP() << "PG_TEST_HOST is not set: no test database";
	core::ConfigReader config(environment);
	const pg::ConnectionSettings settings = pg::readSettings(config, "PG_TEST_");
	const auto checked = config.finish();
	ASSERT_TRUE(checked) << checked.error().message;

	pg::PoolOptions options;
	options.size = this->poolSize();
	options.connectAttempts = 1;
	auto opened = pg::ConnectionPool::open(settings, options, this->logger);
	ASSERT_TRUE(opened) << opened.error().message;
	this->pool = std::move(*opened);

	// Rapide quand tout est déjà appliqué : une somme de contrôle par fichier.
	pg::MigrationOptions migrations;
	migrations.directory = MATCHA_MIGRATIONS_DIR;
	const auto migrated = pg::migrate(*this->pool, migrations, this->logger);
	ASSERT_TRUE(migrated) << migrated.error().message;
}

SavepointExecutor::SavepointExecutor(pg::Transaction &transaction)
	: _transaction(transaction)
{
}

core::Result<pg::Rows> SavepointExecutor::execute(std::string_view sql, const pg::Params &params)
{
	if (const auto saved = this->_transaction.execute("SAVEPOINT statement"); !saved)
		return std::unexpected(saved.error());
	auto result = this->_transaction.execute(sql, params);
	if (!result)
		(void)this->_transaction.execute("ROLLBACK TO SAVEPOINT statement");
	(void)this->_transaction.execute("RELEASE SAVEPOINT statement");
	return result;
}

void RepositoryTest::SetUp()
{
	DatabaseTest::SetUp();
	if (this->IsSkipped() || this->HasFatalFailure())
		return;
	auto begun = this->pool->begin();
	ASSERT_TRUE(begun) << begun.error().message;
	this->_transaction.emplace(std::move(*begun));
	this->_statements.emplace(*this->_transaction);
}