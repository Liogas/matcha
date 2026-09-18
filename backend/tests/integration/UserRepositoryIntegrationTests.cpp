#include <gtest/gtest.h>
#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"

TEST(UserRepositoryIntegrationTest, CanFindExistingUserByEmail)
{
	Database database;
	UserRepository repository(database);

	const std::string email = "integration_test@example.com";
	const std::string password = "fake_hash";

	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	auto insertResult = database.executeParams(
		"INSERT INTO users (email, password_hash) "
		"VALUES ($1, $2)",
		{email, password}
	);
	ASSERT_FALSE(insertResult.isError());
	const auto user = repository.findByEmail(email);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->email, email);
	EXPECT_EQ(user->passwordHash, password);
}