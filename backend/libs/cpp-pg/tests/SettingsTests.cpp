#include <pg/Settings.hpp>
#include <gtest/gtest.h>

TEST(Settings, ReadsPrefixedVariables)
{
	core::MapEnvironment env;
	env.set("DB_HOST", "database");
	env.set("DB_PORT", "5432");
	env.set("DB_NAME", "matcha");
	env.set("DB_USER", "matcha");
	env.set("DB_PASSWORD", "s3cret");
	core::ConfigReader config(env);
	const pg::ConnectionSettings settings = pg::readSettings(config, "DB_");
	ASSERT_TRUE(config.finish());
	EXPECT_EQ(settings.host, "database");
	EXPECT_EQ(settings.port, "5432");
	EXPECT_EQ(settings.password, "s3cret");
}

TEST(Settings, ReportsMissingVariables)
{
	core::MapEnvironment env;
	env.set("DB_HOST", "database");
	core::ConfigReader config(env);
	pg::readSettings(config, "DB_");
	const auto result = config.finish();
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().message,
		"DB_PORT is required; DB_NAME is required; DB_USER is required; "
		"DB_PASSWORD or DB_PASSWORD_FILE is required");
}