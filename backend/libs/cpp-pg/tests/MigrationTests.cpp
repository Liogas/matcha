#include "support/PgTest.hpp"
#include <pg/Migrations.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace
{
	// Un dossier de migrations jetable, propre à chaque test.
	class MigrationDirectory
	{
	public:
		MigrationDirectory()
			: _path(fs::temp_directory_path() / ("cpp-pg-" + std::string(testing::UnitTest::GetInstance()->current_test_info()->name())))
		{
			fs::remove_all(this->_path);
			fs::create_directories(this->_path);
		}
		~MigrationDirectory() { fs::remove_all(this->_path); }

		void write(const std::string &name, const std::string &sql) const
		{
			std::ofstream(this->_path / name) << sql;
		}
		const fs::path &path() const { return this->_path; }

	private:
		fs::path _path;
	};
}

// ---- Sans base : lire le dossier ----

TEST(MigrationFiles, SortedByNumberNotByName)
{
	const MigrationDirectory directory;
	directory.write("10_second.sql", "");
	directory.write("9_first.sql", "");
	directory.write("notes.txt", ""); // pas en .sql : ignoré
	const auto files = pg::listMigrations(directory.path());
	ASSERT_TRUE(files) << files.error().message;
	ASSERT_EQ(files->size(), 2u);
	EXPECT_EQ((*files)[0].version, 9);
	EXPECT_EQ((*files)[1].name, "10_second.sql");
}

TEST(MigrationFiles, RejectsBadNames)
{
	const MigrationDirectory directory;
	directory.write("create_users.sql", "");
	const auto files = pg::listMigrations(directory.path());
	ASSERT_FALSE(files);
	EXPECT_EQ(files.error().code, core::ErrorCode::Invalid);
}

TEST(MigrationFiles, RejectsDuplicateVersions)
{
	const MigrationDirectory directory;
	directory.write("004_add_bio.sql", "");
	directory.write("004_add_city.sql", "");
	const auto files = pg::listMigrations(directory.path());
	ASSERT_FALSE(files);
	EXPECT_EQ(files.error().message, "migrations 004_add_bio.sql and 004_add_city.sql have the same version");
}

// ---- Avec base ----

class Migrations : public PgTest
{
protected:
	void SetUp() override
	{
		PgTest::SetUp();
		if (this->IsSkipped() || this->HasFatalFailure())
			return;
		ASSERT_TRUE(this->connection->executeScript("DROP TABLE IF EXISTS test_migrations, mig_items"));
		this->options.directory = this->directory.path();
		this->options.table = "test_migrations";
	}

	bool tableExists(const std::string &name)
	{
		const auto rows = this->connection->query("SELECT to_regclass($1) IS NOT NULL AS found", name);
		return rows && (*rows)[0].get<bool>("found");
	}

	MigrationDirectory directory;
	pg::MigrationOptions options;
};

TEST_F(Migrations, AppliesPendingFilesOnce)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->directory.write("002_fill.sql", "INSERT INTO mig_items VALUES (1);\nINSERT INTO mig_items VALUES (2);");
	const auto first = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(first) << first.error().message;
	EXPECT_EQ(first->applied, (std::vector<std::string>{"001_create.sql", "002_fill.sql"}));

	const auto second = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(second) << second.error().message;
	EXPECT_TRUE(second->applied.empty());
	EXPECT_EQ(second->upToDate, 2u);
	const auto rows = this->connection->query("SELECT count(*) AS n FROM mig_items");
	ASSERT_TRUE(rows);
	EXPECT_EQ((*rows)[0].get<std::int64_t>("n"), 2);
}

TEST_F(Migrations, FailureAppliesNothing)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->directory.write("002_broken.sql", "INSERT INTO mig_items VALUES ('not a number');");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_NE(result.error().message.find("002_broken.sql"), std::string::npos);
	EXPECT_FALSE(this->tableExists("mig_items")); // 001 annulée aussi
	EXPECT_FALSE(this->tableExists("test_migrations"));
}

TEST_F(Migrations, RefusesAModifiedFile)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	ASSERT_TRUE(pg::migrate(*this->pool, this->options, this->logger));
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id BIGINT);");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().message, "migration 001_create.sql was modified after being applied");
}

TEST_F(Migrations, DryRunChangesNothing)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	this->options.dryRun = true;
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(result) << result.error().message;
	EXPECT_EQ(result->applied, (std::vector<std::string>{"001_create.sql"}));
	EXPECT_FALSE(this->tableExists("mig_items"));
}

TEST_F(Migrations, AdoptsRowsWithoutChecksum)
{
	// La table telle que l'écrivait l'ancien DatabaseInitializer.
	ASSERT_TRUE(this->connection->executeScript(
		"CREATE TABLE test_migrations (version INTEGER PRIMARY KEY, applied_at TIMESTAMPTZ NOT NULL DEFAULT now());"
		"INSERT INTO test_migrations (version) VALUES (1);"
		"CREATE TABLE mig_items (id INT);"));
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT);");
	const auto result = pg::migrate(*this->pool, this->options, this->logger);
	ASSERT_TRUE(result) << result.error().message;
	EXPECT_EQ(result->upToDate, 1u);
	const auto rows = this->connection->query("SELECT checksum FROM test_migrations WHERE version = 1");
	ASSERT_TRUE(rows);
	EXPECT_EQ((*rows)[0].get<std::string>("checksum").size(), 64u); // SHA-256 en hexadécimal
}

// Deux serveurs qui démarrent en même temps : le verrou consultatif fait
// attendre le second, qui trouve ensuite tout déjà appliqué.
TEST_F(Migrations, TwoMigratorsApplyOnce)
{
	this->directory.write("001_create.sql", "CREATE TABLE mig_items (id INT); SELECT pg_sleep(0.2);");
	core::Result<pg::MigrationReport> results[2] = {core::fail(core::ErrorCode::Internal, ""),
													  core::fail(core::ErrorCode::Internal, "")};
	std::thread first([&]
					  { results[0] = pg::migrate(*this->pool, this->options, this->logger); });
	std::thread second([&]
					   { results[1] = pg::migrate(*this->pool, this->options, this->logger); });
	first.join();
	second.join();
	ASSERT_TRUE(results[0]) << results[0].error().message;
	ASSERT_TRUE(results[1]) << results[1].error().message;
	EXPECT_EQ(results[0]->applied.size() + results[1]->applied.size(), 1u);
	EXPECT_EQ(results[0]->upToDate + results[1]->upToDate, 1u);
}