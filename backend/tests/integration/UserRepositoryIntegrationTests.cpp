#include <gtest/gtest.h>
#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"

// TEST 1
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

// TEST 2
TEST(UserRepositoryIntegrationTest, CanCreateUser)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "create_test@example.com";
	const std::string password = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto userId = repository.createUser(
		email,
		password
	);
	ASSERT_TRUE(userId.has_value());
	EXPECT_GT(*userId, 0);
	const auto user = repository.findByEmail(email);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->id, *userId);
	EXPECT_EQ(user->email, email);
	EXPECT_EQ(user->passwordHash, password);
	database.executeParams(
		"DELETE FROM users"
		"WHERE email = $1",
		{email}
	);
}

// TEST 3
TEST(UserRepositoryIntegrationTest, CanFindExistingUserById)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "find_by_id_test@example.com";
	const std::string password = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto createdId = repository.createUser(
		email,
		password
	);
	ASSERT_TRUE(createdId.has_value());
	const auto user = repository.findById(*createdId);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->id, *createdId);
	EXPECT_EQ(user->email, email);
	EXPECT_EQ(user->passwordHash, password);
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}

// TEST 4
TEST(UserRepositoryIntegrationTest, CreateUserFailsWhenEmailAlreadyExists)
{
	Database database;
	UserRepository repository(database);
	const std::string email = "duplicate_test@example.com";
	const std::string pwd = "fake_hash";
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
	const auto firstId = repository.createUser(
		email,
		pwd
	);
	ASSERT_TRUE(firstId.has_value());
	const auto secondId = repository.createUser(
		email,
		pwd
	);
	EXPECT_FALSE(secondId.has_value());
	database.executeParams(
		"DELETE FROM users WHERE email = $1",
		{email}
	);
}