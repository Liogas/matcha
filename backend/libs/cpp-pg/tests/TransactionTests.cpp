#include "support/PgTest.hpp"
#include <latch>
#include <thread>


namespace
{
	std::int64_t countItems(pg::ConnectionPool &pool)
	{
		const auto rows = pool.query("SELECT count(*) AS n FROM items");
		return rows ? (*rows)[0].get<std::int64_t>("n") : -1;
	}
}

class TransactionTest : public PgTest
{
};

TEST_F(TransactionTest, CommitKeepsChanges)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "kept"));
	ASSERT_TRUE(tx->commit());
	EXPECT_EQ(countItems(*this->pool), 1);
}

TEST_F(TransactionTest, DestructorRollsBack)
{
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "forgotten"));
	} // pas de commit() : ROLLBACK ici
	EXPECT_EQ(countItems(*this->pool), 0);
	EXPECT_EQ(this->pool->idleCount(), 2u);
}

TEST_F(TransactionTest, FailedQueryPoisonsTheTransaction)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "first"));
	ASSERT_FALSE(tx->query("INSERT INTO items (name) VALUES ($1)", "first")); // doublon
	const auto next = tx->query("SELECT 1");
	ASSERT_FALSE(next);
	EXPECT_NE(next.error().message.find("25P02"), std::string::npos);
	const auto committed = tx->commit();
	ASSERT_FALSE(committed);
	EXPECT_EQ(countItems(*this->pool), 0);
}

TEST_F(TransactionTest, UseAfterCommitIsABug)
{
	auto tx = this->pool->begin();
	ASSERT_TRUE(tx) << tx.error().message;
	ASSERT_TRUE(tx->commit());
	EXPECT_THROW((void)tx->query("SELECT 1"), std::logic_error);
}

// Deux threads, deux transactions en même temps : chacune ne voit que ses
// propres lignes tant qu'elle n'a pas validé.
TEST_F(TransactionTest, ParallelTransactionsDoNotMix)
{
	std::latch bothInserted(2);
	bool sawOther[2] = {true, true};
	auto worker = [&](int id, bool commit)
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(tx->query("INSERT INTO items (name) VALUES ($1)", "thread-" + std::to_string(id)));
		bothInserted.arrive_and_wait(); // les deux INSERT sont faits, aucun n'est validé
		const auto rows = tx->query("SELECT count(*) AS n FROM items");
		ASSERT_TRUE(rows) << rows.error().message;
		sawOther[id] = (*rows)[0].get<std::int64_t>("n") != 1;
		if (commit)
		{
			ASSERT_TRUE(tx->commit()); // accolades : ASSERT_ cache un if/else
		}
	};
	std::thread first(worker, 0, true);
	std::thread second(worker, 1, false);
	first.join();
	second.join();
	EXPECT_FALSE(sawOther[0]);
	EXPECT_FALSE(sawOther[1]);
	const auto rows = this->pool->query("SELECT name FROM items");
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	EXPECT_EQ((*rows)[0].get<std::string>("name"), "thread-0");
}

// Le même code « repository » (il ne connaît qu'un Executor) tourne seul, ou
// dans une transaction qu'il ne contrôle pas.
namespace
{
	bool insertItem(pg::Executor &database, const std::string &name)
	{
		return database.query("INSERT INTO items (name) VALUES ($1)", name).has_value();
	}
}

TEST_F(TransactionTest, ExecutorRunsInsideTheCallersTransaction)
{
	{
		auto tx = this->pool->begin();
		ASSERT_TRUE(tx) << tx.error().message;
		ASSERT_TRUE(insertItem(*tx, "in-transaction"));
	} // ROLLBACK : la ligne disparaît avec la transaction
	EXPECT_EQ(countItems(*this->pool), 0);
	ASSERT_TRUE(insertItem(*this->pool, "alone"));
	EXPECT_EQ(countItems(*this->pool), 1);
}