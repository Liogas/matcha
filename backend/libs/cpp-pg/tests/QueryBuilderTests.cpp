#include <pg/QueryBuilder.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

// Tests purs : on compare le SQL et les paramètres produits, sans base.

namespace
{
	std::vector<std::string> texts(const pg::Params &params)
	{
		std::vector<std::string> out;
		for (const pg::Text &value : params.values())
			out.push_back(value.value_or("NULL"));
		return out;
	}
}

TEST(QueryBuilder, WithoutConditionsHasNoWhere)
{
	const auto query = pg::QueryBuilder("SELECT id FROM items").build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items");
	EXPECT_EQ(query->params.size(), 0u);
}

TEST(QueryBuilder, NumbersPlaceholdersInOrder)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("score >= ?", 10).where("name = ? OR name = ?", "a", "b");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (score >= $1) AND (name = $2 OR name = $3)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"10", "a", "b"}));
}

TEST(QueryBuilder, WhereIfSkipsEmptyOptional)
{
	const std::optional<int> minScore;
	const std::optional<std::string> name = "a";
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.whereIf(minScore, "score >= ?").whereIf(name, "name = ?");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (name = $1)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"a"}));
}

TEST(QueryBuilder, ValuesNeverEnterTheSql)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("name = ?", "x' OR '1'='1");
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE (name = $1)");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"x' OR '1'='1"}));
}

TEST(QueryBuilder, WrongNumberOfValuesIsABug)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	EXPECT_THROW(builder.where("score BETWEEN ? AND ?", 1), std::logic_error);
	EXPECT_THROW(builder.where("score = 1", 1), std::logic_error);
}

TEST(QueryBuilder, KeysetConditionOnTwoColumns)
{
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.where("(score, id) < (?, ?)", 42, 7).limit(20);
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items WHERE ((score, id) < ($1, $2)) LIMIT $3");
	EXPECT_EQ(texts(query->params), (std::vector<std::string>{"42", "7", "20"}));
}

TEST(QueryBuilder, OrderByComesFromTheWhitelist)
{
	const pg::SortWhitelist sorts{{"newest", "id DESC"}, {"score", "score DESC, id DESC"}};
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.orderBy("score", sorts).limit(5);
	const auto query = builder.build();
	ASSERT_TRUE(query);
	EXPECT_EQ(query->sql, "SELECT id FROM items ORDER BY score DESC, id DESC LIMIT $1");
}

TEST(QueryBuilder, UnknownSortKeyIsInvalid)
{
	const pg::SortWhitelist sorts{{"newest", "id DESC"}};
	pg::QueryBuilder builder("SELECT id FROM items");
	builder.orderBy("id; DROP TABLE items", sorts);
	const auto query = builder.build();
	ASSERT_FALSE(query);
	EXPECT_EQ(query.error().code, core::ErrorCode::Invalid);
	EXPECT_EQ(query.error().message, "unknown sort key 'id; DROP TABLE items'");
}