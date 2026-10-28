#include "support/PgTest.hpp"

using namespace std::chrono;

TEST(Connection, ReportsUnreachableServer)
{
	pg::ConnectionSettings settings;
	settings.host = "127.0.0.1";
	settings.port = "1"; // personne n'écoute ici
	const auto connection = pg::Connection::open(settings);
	ASSERT_FALSE(connection);
	EXPECT_EQ(connection.error().code, core::ErrorCode::Unavailable);
}

class Query : public PgTest
{
};

TEST_F(Query, ReadsTypedColumnsByName)
{
	const auto rows = this->connection->query(
		"SELECT $1::bigint AS id, $2::text AS name, $3::boolean AS active", 42, "alice", true);
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	const pg::Row row = (*rows)[0];
	EXPECT_EQ(row.get<std::int64_t>("id"), 42);
	EXPECT_EQ(row.get<std::string>("name"), "alice");
	EXPECT_TRUE(row.get<bool>("active"));
}

TEST_F(Query, NullNeedsAnOptional)
{
	const auto rows = this->connection->query("SELECT $1::integer AS score", std::optional<int>{});
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_EQ((*rows)[0].get<std::optional<int>>("score"), std::nullopt);
	EXPECT_THROW((*rows)[0].get<int>("score"), pg::RowError);
}

TEST_F(Query, UnknownColumnIsABug)
{
	const auto rows = this->connection->query("SELECT 1 AS id");
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_THROW((*rows)[0].get<int>("idd"), pg::RowError);
}

TEST_F(Query, ReadsTimestamptz)
{
	const auto rows = this->connection->query(
		"SELECT TIMESTAMPTZ '2026-10-27 10:30:00.123456+02' AS at, $1::timestamptz AS echo",
		pg::Timestamp{sys_days{2026y / October / 27} + 8h});
	ASSERT_TRUE(rows) << rows.error().message;
	EXPECT_EQ((*rows)[0].get<pg::Timestamp>("at"),
			  sys_days{2026y / October / 27} + 8h + 30min + 123456us);
	EXPECT_EQ((*rows)[0].get<pg::Timestamp>("echo"), sys_days{2026y / October / 27} + 8h);
}

TEST_F(Query, ParametersAreNeverSql)
{
	const std::string evil = "x'); DROP TABLE items; --";
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ($1)", evil));
	const auto rows = this->connection->query("SELECT name FROM items");
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 1u);
	EXPECT_EQ((*rows)[0].get<std::string>("name"), evil);
}

TEST_F(Query, ArrayParameter)
{
	ASSERT_TRUE(this->connection->query(
		"INSERT INTO items (name) SELECT unnest($1::text[])",
		std::vector<std::string>{"a", "b \"quoted\"", "c"}));
	const auto rows = this->connection->query(
		"SELECT name FROM items WHERE name = ANY($1) ORDER BY id",
		std::vector<std::string>{"a", "b \"quoted\""});
	ASSERT_TRUE(rows) << rows.error().message;
	ASSERT_EQ(rows->size(), 2u);
	EXPECT_EQ((*rows)[1].get<std::string>("name"), "b \"quoted\"");
}

TEST_F(Query, MapsSqlErrors)
{
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ($1)", "alice"));
	const auto duplicate = this->connection->query("INSERT INTO items (name) VALUES ($1)", "alice");
	ASSERT_FALSE(duplicate);
	EXPECT_EQ(duplicate.error().code, core::ErrorCode::Conflict);
	EXPECT_EQ(duplicate.error().message,
			  "duplicate key value violates unique constraint \"items_name_key\" (SQLSTATE 23505)");

	const auto negative = this->connection->query(
		"INSERT INTO items (name, score) VALUES ($1, $2)", "bob", -1);
	ASSERT_FALSE(negative);
	EXPECT_EQ(negative.error().code, core::ErrorCode::Invalid);

	const auto typo = this->connection->query("SELEC 1");
	ASSERT_FALSE(typo);
	EXPECT_EQ(typo.error().code, core::ErrorCode::Internal);
}

TEST_F(Query, CountsAffectedRows)
{
	ASSERT_TRUE(this->connection->query("INSERT INTO items (name) VALUES ('a'), ('b'), ('c')"));
	const auto updated = this->connection->query("UPDATE items SET score = 1 WHERE name <> $1", "a");
	ASSERT_TRUE(updated) << updated.error().message;
	EXPECT_EQ(updated->affected(), 2);
	EXPECT_TRUE(updated->empty());
}