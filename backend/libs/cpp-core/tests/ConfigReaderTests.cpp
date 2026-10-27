#include <core/ConfigReader.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
	std::filesystem::path writeFile(const std::string &name, const std::string &content)
	{
		const auto path = std::filesystem::temp_directory_path() / name;
		std::ofstream(path, std::ios::binary) << content;
		return path;
	}
}

TEST(ConfigReader, ReadsRequiredValue)
{
	core::MapEnvironment env;
	env.set("HOST", "db");
	core::ConfigReader config(env);
	EXPECT_EQ(config.required("HOST"), "db");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ReportsEveryMissingVariableAtOnce)
{
	core::MapEnvironment env;
	env.set("EMPTY", "");
	core::ConfigReader config(env);
	config.required("FIRST");
	config.required("EMPTY");
	config.requiredInt("PORT");
	const auto result = config.finish();
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(result.error().message,
			  "FIRST is required; EMPTY is required; PORT is required");
}

TEST(ConfigReader, OptionalFallsBackToDefault)
{
	core::MapEnvironment env;
	core::ConfigReader config(env);
	EXPECT_EQ(config.optional("NAME", "matcha"), "matcha");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ParsesIntegers)
{
	core::MapEnvironment env;
	env.set("OK", "6379");
	env.set("BAD", "63x");
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredInt("OK"), 6379);
	config.requiredInt("BAD");
	EXPECT_EQ(config.finish().error().message, "BAD must be an integer, got '63x'");
}

TEST(ConfigReader, ParsesBooleansStrictly)
{
	core::MapEnvironment env;
	env.set("ON", "true");
	env.set("OFF", "false");
	env.set("BAD", "yes");
	core::ConfigReader config(env);
	EXPECT_TRUE(config.optionalBool("ON", false));
	EXPECT_FALSE(config.optionalBool("OFF", true));
	EXPECT_TRUE(config.optionalBool("MISSING", true));
	config.optionalBool("BAD", false);
	EXPECT_EQ(config.finish().error().message, "BAD must be 'true' or 'false', got 'yes'");
}

TEST(ConfigReader, ReadsSecretFromVariable)
{
	core::MapEnvironment env;
	env.set("DB_PASSWORD", "direct");
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredSecret("DB_PASSWORD"), "direct");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, ReadsSecretFromFileWithoutTrailingNewline)
{
	const auto path = writeFile("cpp-core-secret", "s3cret\n");
	core::MapEnvironment env;
	env.set("DB_PASSWORD_FILE", path.string());
	core::ConfigReader config(env);
	EXPECT_EQ(config.requiredSecret("DB_PASSWORD"), "s3cret");
	EXPECT_TRUE(config.finish());
}

TEST(ConfigReader, RejectsBothSecretSources)
{
	core::MapEnvironment env;
	env.set("DB_PASSWORD", "a");
	env.set("DB_PASSWORD_FILE", "/x");
	core::ConfigReader config(env);
	config.requiredSecret("DB_PASSWORD");
	EXPECT_FALSE(config.finish());
}

TEST(ConfigReader, RejectsMissingOrEmptySecretFile)
{
	const auto empty = writeFile("cpp-core-empty-secret", "\n");
	core::MapEnvironment env;
	env.set("A_FILE", "/nonexistent/secret");
	env.set("B_FILE", empty.string());
	core::ConfigReader config(env);
	config.requiredSecret("A");
	config.requiredSecret("B");
	config.requiredSecret("C");
	EXPECT_EQ(config.finish().error().message,
			  "A_FILE: cannot read /nonexistent/secret; B_FILE: " + empty.string() +
				  " is empty; C or C_FILE is required");
}

TEST(SystemEnvironment, ReadsProcessEnvironment)
{
	::setenv("CPP_CORE_TEST_VARIABLE", "value", 1);
	::setenv("CPP_CORE_TEST_EMPTY", "", 1);
	const core::SystemEnvironment env;
	EXPECT_EQ(env.get("CPP_CORE_TEST_VARIABLE"), "value");
	EXPECT_EQ(env.get("CPP_CORE_TEST_EMPTY"), std::nullopt);
	EXPECT_EQ(env.get("CPP_CORE_TEST_MISSING"), std::nullopt);
}