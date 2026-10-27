#include <core/Logger.hpp>
#include <gtest/gtest.h>
#include <sstream>

TEST(StreamLogger, WritesOneLogfmtLine)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.info("Server started", {{"port", "18080"}, {"request_id", "abc"}});
	EXPECT_EQ(out.str(), "level=INFO msg=\"Server started\" port=18080 request_id=abc\n");
}

TEST(StreamLogger, SkipsMessagesBelowMinimumLevel)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Warning);
	logger.info("hidden");
	logger.error("shown");
	EXPECT_EQ(out.str(), "level=ERROR msg=shown\n");
}

TEST(StreamLogger, EscapesNewlinesAndQuotes)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.warning("bad\nlevel=ERROR msg=fake", {{"name", "say \"hi\""}});
	EXPECT_EQ(out.str(),
			  "level=WARNING msg=\"bad\\nlevel=ERROR msg=fake\" name=\"say \\\"hi\\\"\"\n");
}

TEST(StreamLogger, MasksSensitiveFields)
{
	std::ostringstream out;
	core::StreamLogger logger(out, core::LogLevel::Debug);
	logger.info("login", {{"password", "hunter2"},
						  {"refresh_token", "abc"},
						  {"DB_SECRET", "x"},
						  {"user", "alice"}});
	EXPECT_EQ(out.str(),
			  "level=INFO msg=login password=*** refresh_token=*** DB_SECRET=*** user=alice\n");
}