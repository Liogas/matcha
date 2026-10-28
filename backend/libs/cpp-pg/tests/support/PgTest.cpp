#include "PgTest.hpp"
#include <core/ConfigReader.hpp>

void PgTest::SetUp()
{
	const core::SystemEnvironment environment;
	if (!environment.get("PG_TEST_HOST"))
		GTEST_SKIP() << "PG_TEST_HOST is not set: no test database";
	core::ConfigReader config(environment);
	this->settings = pg::readSettings(config, "PG_TEST_");
	const auto checked = config.finish();
	ASSERT_TRUE(checked) << checked.error().message;

	auto opened = pg::Connection::open(this->settings);
	ASSERT_TRUE(opened) << opened.error().message;
	this->connection = std::move(*opened);
	ASSERT_TRUE(this->connection->query(
		"CREATE TABLE IF NOT EXISTS items ("
		"  id         BIGSERIAL PRIMARY KEY,"
		"  name       TEXT NOT NULL UNIQUE,"
		"  score      INTEGER CHECK (score >= 0),"
		"  created_at TIMESTAMPTZ NOT NULL DEFAULT now())"));
	ASSERT_TRUE(this->connection->query("TRUNCATE items RESTART IDENTITY"));
	this->pool = this->openPool(2);
	ASSERT_NE(this->pool, nullptr);
}

std::unique_ptr<pg::ConnectionPool> PgTest::openPool(std::size_t size)
{
	pg::PoolOptions options;
	options.size = size;
	options.acquireTimeout = std::chrono::milliseconds{200};
	options.connectAttempts = 1;
	auto pool = pg::ConnectionPool::open(this->settings, options, this->logger);
	if (!pool)
	{
		ADD_FAILURE() << pool.error().message;
		return nullptr;
	}
	return std::move(*pool);
}