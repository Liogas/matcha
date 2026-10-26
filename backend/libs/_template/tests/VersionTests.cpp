#include <tmpl/Version.hpp>
#include <gtest/gtest.h>
#include <regex>
#include <string>

TEST(Version, FollowsSemanticVersioning)
{
	const std::string v{tmpl::version()};
	EXPECT_TRUE(
    	std::regex_match(v, std::regex{R"(\d+\.\d+\.\d+)"})
	) << v;
}