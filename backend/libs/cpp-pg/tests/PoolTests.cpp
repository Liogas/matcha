#include "support/PgTest.hpp"
#include <thread>

using namespace std::chrono;

class Pool : public PgTest
{
};

TEST_F(Pool, ExhaustedPoolTimesOut)
{
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	const auto start = steady_clock::now();
	const auto third = this->pool->acquire(milliseconds{100});
	const auto waited = steady_clock::now() - start;
	ASSERT_FALSE(third);
	EXPECT_EQ(third.error().code, core::ErrorCode::Unavailable);
	EXPECT_GE(waited, milliseconds{100});
}

TEST_F(Pool, WaiterGetsReleasedConnection)
{
	std::optional<pg::PooledConnection> first(*this->pool->acquire());
	std::optional<pg::PooledConnection> second(*this->pool->acquire());
	std::thread releaser([&]
						 {
							 std::this_thread::sleep_for(milliseconds{50});
							 first.reset(); // rend la connexion au pool
						 });
	const auto third = this->pool->acquire(seconds{2});
	releaser.join();
	EXPECT_TRUE(third);
}

TEST_F(Pool, ReleaseRollsBackForgottenTransaction)
{
	{
		auto connection = this->pool->acquire();
		ASSERT_TRUE(connection);
		ASSERT_TRUE((*connection)->query("BEGIN"));
		ASSERT_TRUE((*connection)->query("INSERT INTO items (name) VALUES ('ghost')"));
	} // rendue au pool au milieu d'une transaction
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	EXPECT_FALSE((*first)->inTransaction());
	EXPECT_FALSE((*second)->inTransaction());
}

TEST_F(Pool, ReconnectsBrokenConnection)
{
	{
		auto victim = this->pool->acquire();
		ASSERT_TRUE(victim);
		const auto pid = (*victim)->query("SELECT pg_backend_pid() AS pid");
		ASSERT_TRUE(pid);
		auto killer = pg::Connection::open(this->settings);
		ASSERT_TRUE(killer);
		ASSERT_TRUE(killer->query("SELECT pg_terminate_backend($1)",
								  (*pid)[0].get<int>("pid")));
		const auto lost = (*victim)->query("SELECT 1");
		ASSERT_FALSE(lost);
		EXPECT_EQ(lost.error().code, core::ErrorCode::Unavailable);
	} // rendue cassée : le pool la reconnecte
	auto first = this->pool->acquire();
	auto second = this->pool->acquire();
	ASSERT_TRUE(first && second);
	EXPECT_TRUE((*first)->query("SELECT 1"));
	EXPECT_TRUE((*second)->query("SELECT 1"));
	EXPECT_NE(this->logs.str().find("PostgreSQL connection lost"), std::string::npos);
}